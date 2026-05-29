// main_window_scroll.cpp  —  l-reader
//
// Responsabilidades:
//   • Scroll cinético (roda do mouse) via timer de 60 fps
//   • Zoom via Ctrl+roda (multiplicativo com suporte a trackpad)
//   • smoothScrollTo() — animação de scroll programático
//   • eventFilter() — intercepta eventos de roda no viewport
//   • wireScrollSignals()

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include <QApplication>
#include <QCursor>
#include <QEasingCurve>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

// ── Scroll cinético ──────────────────────────────────────────────────────────
// kKineticImpulse  — velocidade inicial injetada por notch padrão (120 delta).
// kKineticFriction — fator de decaimento por frame a 60 fps.
//   0.82 → para em ~14 frames (230 ms) a partir de 1 notch.
// kKineticMinVelocity — threshold abaixo do qual o timer para (px/frame).
static constexpr qreal kKineticImpulse = 22.0;
static constexpr qreal kKineticFriction = 0.82;
static constexpr qreal kKineticMinVelocity = 0.8;
static constexpr int kKineticIntervalMs = 16; // ~60 fps

// ── Zoom Ctrl+roda ───────────────────────────────────────────────────────────
// 1.15 ≈ 15% por notch — responsivo sem pular demais.
static constexpr qreal kZoomWheelFactor = 1.15;

// ── Scroll programático ──────────────────────────────────────────────────────
static constexpr int kScrollAnimMs = 320;

// ── Zoom debounce ────────────────────────────────────────────────────────────
// Tempo de silêncio (ms) antes de atualizar o DPI do PageCache.
static constexpr int kZoomDebounceMs = 230;

// ─────────────────────────────────────────────────────────────────────────────
// wireScrollSignals — inicializa timers e conecta scroll/zoom
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireScrollSignals() {
  connect(m_pdfView, &PdfCanvasView::currentPageChanged, m_pageCache.get(),
          &PageCache::onCurrentPageChanged);

  connect(m_pdfScroll->verticalScrollBar(), &QScrollBar::valueChanged,
          m_pdfView, &PdfCanvasView::onScrollChanged);

  // Timer cinético (~60 fps)
  m_scrollKineticTimer = new QTimer(this);
  m_scrollKineticTimer->setInterval(kKineticIntervalMs);
  m_scrollKineticTimer->setTimerType(Qt::PreciseTimer);
  connect(m_scrollKineticTimer, &QTimer::timeout, this,
          &MainWindow::onScrollTick);

  // Animação suave para navegação programática
  m_scrollAnim = new QVariantAnimation(this);
  m_scrollAnim->setDuration(kScrollAnimMs);
  m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_scrollAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            m_pdfScroll->verticalScrollBar()->setValue(v.toInt());
          });

  // Zoom debounce
  m_zoomDebounceTimer = new QTimer(this);
  m_zoomDebounceTimer->setSingleShot(true);
  m_zoomDebounceTimer->setInterval(kZoomDebounceMs);
  connect(m_zoomDebounceTimer, &QTimer::timeout, this,
          &MainWindow::onZoomSettled);

  m_pdfView->installEventFilter(this);
  m_pdfScroll->viewport()->installEventFilter(this);
}

// ─────────────────────────────────────────────────────────────────────────────
// eventFilter — intercepta eventos de roda para scroll cinético e Ctrl+zoom
// ─────────────────────────────────────────────────────────────────────────────
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  // ── Redimensionamento do dock sidebar — actualiza espaçador da barra
  // inferior
  if (obj == m_sidebar && event->type() == QEvent::Resize)
    updateBottomNavOffset();

  // ── Auto-hide das barras em Modo Casual ──────────────────────────────
  // Captura movimentos do rato registados globalmente via
  // qApp->installEventFilter
  if (m_casualAutoHide && event->type() == QEvent::MouseMove) {
    const QPoint localPos = mapFromGlobal(QCursor::pos());
    if (rect().contains(localPos))
      onCasualMouseEdge(localPos.y());
    // Não consome o evento — propaga normalmente
  }

  const bool isScrollTarget =
      (obj == m_pdfView || obj == m_pdfScroll->viewport());
  if (!isScrollTarget || event->type() != QEvent::Wheel)
    return QMainWindow::eventFilter(obj, event);

  auto *we = static_cast<QWheelEvent *>(event);

  if (we->modifiers() & Qt::ControlModifier) {
    // Cancela scroll cinético — usuário mudou intenção para zoom
    m_scrollVelocity = 0.0;
    m_scrollKineticTimer->stop();

    // Zoom multiplicativo ancorado ao cursor.
    // std::pow interpola o fator para notches parciais de trackpad:
    //   delta=120  → fator = 1.15^1   = +15%
    //   delta=60   → fator = 1.15^0.5 ≈ +7.2%  (trackpad suave)
    //   delta=-120 → fator = 1.15^-1  ≈ -13%
    const qreal delta = we->angleDelta().y();
    const qreal factor = std::pow(kZoomWheelFactor, delta / 120.0);
    const int anchorY = static_cast<int>(we->position().y());
    m_pdfView->applyZoomFactor(factor, anchorY);
    return true;
  }

  const qreal delta = -we->angleDelta().y();
  const qreal impulse = (delta / 120.0) * kKineticImpulse;

  // Cancela animação programática para que o usuário retome o controle
  if (m_scrollAnim->state() == QAbstractAnimation::Running)
    m_scrollAnim->stop();

  // Inversão de direção: amortece antes de aplicar impulso oposto
  if ((impulse > 0 && m_scrollVelocity < 0) ||
      (impulse < 0 && m_scrollVelocity > 0))
    m_scrollVelocity *= 0.3;

  m_scrollVelocity += impulse;

  if (!m_scrollKineticTimer->isActive())
    m_scrollKineticTimer->start();

  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// onScrollTick — motor do scroll cinético, chamado a ~60 fps.
//
// Modelo de física:
//   1. Aplica m_scrollVelocity (px/frame) à scrollbar.
//   2. Decai: velocity *= kKineticFriction   (decaimento exponencial).
//   3. Para o timer quando |velocity| < kKineticMinVelocity.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onScrollTick() {
  QScrollBar *vsb = m_pdfScroll->verticalScrollBar();
  const int current = vsb->value();
  const int next =
      std::clamp(current + static_cast<int>(std::round(m_scrollVelocity)),
                 vsb->minimum(), vsb->maximum());

  vsb->setValue(next);

  m_scrollVelocity *= kKineticFriction;

  const bool atLimit = (next == vsb->minimum() || next == vsb->maximum());
  if (std::abs(m_scrollVelocity) < kKineticMinVelocity || atLimit) {
    m_scrollVelocity = 0.0;
    m_scrollKineticTimer->stop();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// smoothScrollTo — navegação programática (TOC, marcadores, goToPage).
// Cancela qualquer scroll cinético ativo antes de animar.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::smoothScrollTo(int targetY) {
  m_scrollVelocity = 0.0;
  m_scrollKineticTimer->stop();

  QScrollBar *vsb = m_pdfScroll->verticalScrollBar();
  const int clamped = std::clamp(targetY, vsb->minimum(), vsb->maximum());
  if (m_scrollAnim->state() == QAbstractAnimation::Running)
    m_scrollAnim->stop();
  m_scrollAnim->setStartValue(vsb->value());
  m_scrollAnim->setEndValue(clamped);
  m_scrollAnim->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// enableCasualAutoHide — ativa/desativa o auto-hide das barras em Modo Casual
//
// Ao ativar:
//   • Instala filtro global em qApp para capturar QEvent::MouseMove
//   • Cria (uma única vez) os timers de ocultação com 2 s de delay
//
// Ao desativar:
//   • Remove o filtro global
//   • Para os timers
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::enableCasualAutoHide(bool enable) {
  if (m_casualAutoHide == enable)
    return;

  m_casualAutoHide = enable;

  if (enable) {
    // Cria timers na primeira ativação (singleton por instância)
    if (!m_casualToolbarTimer) {
      m_casualToolbarTimer = new QTimer(this);
      m_casualToolbarTimer->setSingleShot(true);
      m_casualToolbarTimer->setInterval(200);
      connect(m_casualToolbarTimer, &QTimer::timeout, this, [this] {
        if (!m_casualAutoHide || !m_toolbar)
          return;
        // Não esconde se o mouse ainda está sobre a toolbar ou se
        // há um popup (menu) aberto — aguarda mais 2 s
        if (!m_toolbar->underMouse() &&
            QApplication::activePopupWidget() == nullptr)
          m_toolbar->hide();
        else
          m_casualToolbarTimer->start();
      });
    }
    if (!m_casualBottomNavTimer) {
      m_casualBottomNavTimer = new QTimer(this);
      m_casualBottomNavTimer->setSingleShot(true);
      m_casualBottomNavTimer->setInterval(200);
      connect(m_casualBottomNavTimer, &QTimer::timeout, this, [this] {
        if (!m_casualAutoHide || !m_bottomNav)
          return;
        if (!m_bottomNav->underMouse() &&
            QApplication::activePopupWidget() == nullptr)
          m_bottomNav->hide();
        else
          m_casualBottomNavTimer->start();
      });
    }

    // Regista filtro global para capturar movimentos do rato em qualquer widget
    qApp->installEventFilter(this);

  } else {
    qApp->removeEventFilter(this);
    if (m_casualToolbarTimer)
      m_casualToolbarTimer->stop();
    if (m_casualBottomNavTimer)
      m_casualBottomNavTimer->stop();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// onCasualMouseEdge — reage à posição vertical do rato dentro da janela
//
// Zonas activas (em pixeis a partir da borda):
//   topo   < kTopZone    → mostra toolbar principal; cancela timer de ocultação
//   base   > h-kBotZone  → mostra barra inferior;   cancela timer de ocultação
//
// Fora das zonas e com a barra visível: inicia o timer de ocultação (2 s)
// se ainda não estiver a contar.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onCasualMouseEdge(int localY) {
  static constexpr int kTopZone = 56; // px a partir do topo da janela
  static constexpr int kBotZone = 56; // px a partir da base da janela

  // ── Toolbar superior ─────────────────────────────────────────────────
  if (m_toolbar && m_casualToolbarTimer) {
    const bool inTopZone = (localY >= 0 && localY < kTopZone);
    if (inTopZone) {
      // Mostra e cancela qualquer contagem regressiva de ocultação
      m_toolbar->show();
      m_casualToolbarTimer->stop();
    } else if (m_toolbar->isVisible() && !m_casualToolbarTimer->isActive()) {
      // Mouse saiu da zona; inicia contagem regressiva
      m_casualToolbarTimer->start();
    }
  }

  // ── Barra de navegação inferior ──────────────────────────────────────
  if (m_bottomNav && m_casualBottomNavTimer) {
    const bool inBotZone = (localY >= height() - kBotZone);
    if (inBotZone) {
      repositionBottomNav(); // garante geometria correcta antes de mostrar
      m_bottomNav->show();
      m_bottomNav->raise();
      m_casualBottomNavTimer->stop();
    } else if (m_bottomNav->isVisible() &&
               !m_casualBottomNavTimer->isActive()) {
      m_casualBottomNavTimer->start();
    }
  }
}
