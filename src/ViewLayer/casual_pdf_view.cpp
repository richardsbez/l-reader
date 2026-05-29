// casual_pdf_view.cpp  —  l-reader

#include "casual_pdf_view.hpp"

#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWidget>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
CasualPdfView::CasualPdfView(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setCursor(Qt::IBeamCursor);

  // ── Fade-in ao virar página ─────────────────────────────────────────────
  // Duração reduzida para 80ms — suficiente para suavizar sem delay
  // perceptível. Se a página já estava em cache (isReady), scheduleFadeIn()
  // pula o fade e exibe instantaneamente (ver implementação abaixo).
  m_fadeAnim = new QPropertyAnimation(this, "opacity", this);
  m_fadeAnim->setDuration(80);
  m_fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

  // ── Spinner de carregamento com atraso ──────────────────────────────────
  // O spinner só aparece se o render demorar mais de kSpinnerDelayMs.
  // Isso evita um flash de spinner em páginas pré-cacheadas ou rápidas.
  m_spinnerDelayTimer = new QTimer(this);
  m_spinnerDelayTimer->setSingleShot(true);
  m_spinnerDelayTimer->setInterval(kSpinnerDelayMs);
  connect(m_spinnerDelayTimer, &QTimer::timeout, this, [this] {
    // Só exibe spinner se ainda não temos as páginas
    const bool leftMissing = !m_leftPx;
    const bool rightMissing = !m_rightPx && (m_leftPage + 1 < m_pageCount);
    if (leftMissing || rightMissing) {
      m_showSpinner = true;
      m_spinnerTimer->start();
    }
  });

  m_spinnerTimer = new QTimer(this);
  m_spinnerTimer->setInterval(14); // ~71 fps — spinner fluido
  connect(m_spinnerTimer, &QTimer::timeout, this, [this] {
    m_spinnerAngle = (m_spinnerAngle + 7) % 360; // 7° por frame = ~500°/s
    const bool leftDone = m_leftPx.has_value();
    const bool rightDone =
        m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);
    if (leftDone && rightDone) {
      m_spinnerTimer->stop();
      m_showSpinner = false;
      update();
    } else {
      update();
    }
  });

  // ── Camada de texto ─────────────────────────────────────────────────────
  // pageRectForIndex retorna {} para páginas fora do spread atual —
  // a camada itera com segurança (não processa páginas invisíveis).
  // scaleForPage calcula a escala real de fit-to-area por página.
  m_textLayer = std::make_unique<PdfTextLayer>(
      [this](int i) -> QRect { return pageRectForIndex(i); },
      [this](int i) -> qreal { return scaleForPage(i); }, this);

  connect(m_textLayer.get(), &PdfTextLayer::repaintAll, this,
          [this] { update(); });
  connect(m_textLayer.get(), &PdfTextLayer::repaintNeeded, this,
          [this](QRect r) { update(r); });
  connect(m_textLayer.get(), &PdfTextLayer::cursorChanged, this,
          [this](Qt::CursorShape s) { setCursor(s); });
  connect(m_textLayer.get(), &PdfTextLayer::textSelected, this,
          &CasualPdfView::textSelected);
  connect(m_textLayer.get(), &PdfTextLayer::highlightRequested, this,
          &CasualPdfView::highlightRequested);
  connect(m_textLayer.get(), &PdfTextLayer::removeHighlightRequested, this,
          &CasualPdfView::removeHighlightRequested);
}

// ─────────────────────────────────────────────────────────────────────────────
// activateDpi / deactivateDpi
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::activateDpi() {
  if (!m_cache)
    return;
  if (m_activeDpi == kCasualDpi)
    return;
  m_activeDpi = kCasualDpi;
  m_cache->setDpi(kCasualDpi);
  m_leftPx.reset();
  m_rightPx.reset();
  requestCurrentPages();
}

void CasualPdfView::deactivateDpi() { m_activeDpi = 0.0; }

// ─────────────────────────────────────────────────────────────────────────────
// setController — liga o CasualModeController ao footer QML.
//
// Cria (na primeira chamada) um QQuickWidget filho que hospeda o
// casual_mode_footer.qml como overlay na parte inferior do view.
// O mesmo controller já usado pelo CasualModeWidget (EPUB) é reutilizado
// aqui para PDF — todas as propriedades (progresso, cores, página) já estão
// sincronizadas pela MainWindow.
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::setController(CasualModeController *ctrl) {
  m_controller = ctrl;

  if (!m_footerWidget) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    m_footerWidget = new QQuickWidget(this);
    m_footerWidget->setObjectName(QStringLiteral("casualPdfFooter"));
    // Fundo opaco — transparência via QML (casualCtrl.headerBg já tem a cor)
    m_footerWidget->setAttribute(Qt::WA_OpaquePaintEvent);
    m_footerWidget->setAttribute(Qt::WA_NoSystemBackground, false);
    m_footerWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_footerWidget->rootContext()->setContextProperty(
        QStringLiteral("casualCtrl"), m_controller);

    m_footerWidget->setSource(QUrl(
        QStringLiteral("qrc:/LReader/LReader/Casual/casual_mode_footer.qml")));
  } else {
    m_footerWidget->rootContext()->setContextProperty(
        QStringLiteral("casualCtrl"), m_controller);
  }

  // Posiciona ANTES de show() para que o widget já apareça no lugar certo
  repositionFooter();
  m_footerWidget->show();
  m_footerWidget->raise();
}

// ─────────────────────────────────────────────────────────────────────────────
// repositionFooter — posiciona o footer QML na borda inferior do widget.
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::repositionFooter() {
  if (!m_footerWidget)
    return;
  m_footerWidget->setGeometry(0, height() - kFooterHeight, width(),
                              kFooterHeight);
  m_footerWidget->raise();
}

// ─────────────────────────────────────────────────────────────────────────────
// setPageCache
//
// Agora recebe Poppler::Document* para habilitar a extração de texto.
// Pré-carrega os tamanhos de página (uma vez por documento) para que
// pageRectForIndex() e scaleForPage() possam trabalhar sem tocar no Poppler
// durante o rendering e a seleção de texto.
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::setPageCache(PageCache *cache, Poppler::Document *doc,
                                 int pageCount) {
  if (m_cache)
    disconnect(m_cache, nullptr, this, nullptr);

  m_cache = cache;
  m_doc = doc;
  m_pageCount = pageCount;
  m_leftPage = 0;
  m_leftPx.reset();
  m_rightPx.reset();

  // ── Pré-carrega tamanhos das páginas em pontos PDF ──────────────────────
  // Usado por pageRectForIndex() para calcular o rect de fit-to-area
  // e por scaleForPage() para derivar a escala real de cada página.
  m_pageSizes.clear();
  m_pageSizes.reserve(pageCount);
  for (int i = 0; i < pageCount; ++i) {
    QSizeF sz;
    if (doc) {
      if (auto pg = std::unique_ptr<Poppler::Page>(doc->page(i)))
        sz = pg->pageSizeF();
    }
    if (sz.isEmpty())
      sz = QSizeF(595.0, 842.0); // A4 como fallback
    m_pageSizes.append(sz);
  }

  // Propaga o documento para a camada de texto
  m_textLayer->setDocument(doc, pageCount);

  if (m_cache) {
    connect(m_cache, &PageCache::pageReady, this, &CasualPdfView::onPageReady);
  }
  update();
}

// ─────────────────────────────────────────────────────────────────────────────
// setBackgroundColor
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::setBackgroundColor(const QColor &c) {
  m_bg = c;
  update();
}

// ─────────────────────────────────────────────────────────────────────────────
// goToSpread
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::goToSpread(int leftPage) {
  if (!m_cache || m_pageCount <= 0)
    return;

  leftPage = std::clamp((leftPage / 2) * 2, 0, m_pageCount - 1);
  if (leftPage == m_leftPage && (m_leftPx || m_rightPx))
    return;

  m_leftPage = leftPage;
  m_leftPx.reset();
  m_rightPx.reset();

  // Limpa seleção: páginas visíveis mudaram
  m_textLayer->clearSelection();

  // Para timers anteriores
  m_spinnerTimer->stop();
  m_spinnerDelayTimer->stop();
  m_showSpinner = false;
  m_spinnerAngle = 0;

  // Tenta obter do cache antes de qualquer coisa
  requestCurrentPages();

  const bool leftReady = m_leftPx.has_value();
  const bool rightReady =
      m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);

  if (leftReady && rightReady) {
    // Ambas em cache: exibição instantânea, sem fade nem spinner.
    m_opacity = 1.0;
    update();
  } else {
    // Cache miss: fade suave + spinner com atraso de kSpinnerDelayMs.
    scheduleFadeIn();
    m_spinnerDelayTimer->start();
    update();
  }

  prefetchAdjacentSpreads();
  emit spreadChanged(m_leftPage, m_leftPage + 1);
}

void CasualPdfView::nextSpread() { goToSpread(m_leftPage + 2); }
void CasualPdfView::prevSpread() { goToSpread(m_leftPage - 2); }

// ─────────────────────────────────────────────────────────────────────────────
// requestCurrentPages / prefetchAdjacentSpreads
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::requestCurrentPages() {
  if (!m_cache)
    return;

  m_leftPx = m_cache->get(m_leftPage);
  m_rightPx = m_leftPage + 1 < m_pageCount ? m_cache->get(m_leftPage + 1)
                                           : std::nullopt;

  // prioritizeRender: máxima prioridade para as páginas atuais.
  // Diferente de requestRender, não duplica se já in-flight.
  if (!m_leftPx)
    m_cache->prioritizeRender(m_leftPage);
  if (!m_rightPx && m_leftPage + 1 < m_pageCount)
    m_cache->prioritizeRender(m_leftPage + 1);

  // Se as páginas já estavam em cache, para spinner e delay imediatamente.
  const bool leftDone = m_leftPx.has_value();
  const bool rightDone =
      m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);
  if (leftDone && rightDone) {
    m_spinnerTimer->stop();
    m_spinnerDelayTimer->stop();
    m_showSpinner = false;
  }

  update();
}

void CasualPdfView::prefetchAdjacentSpreads() {
  if (!m_cache)
    return;
  // Delega ao PageCache que agora tem janela expandida (PRE_FORWARD=10,
  // PRE_BACK=4). onCurrentPageChanged agenda as páginas com prioridade
  // decrescente por distância, garantindo que as mais próximas sejam
  // renderizadas primeiro.
  m_cache->onCurrentPageChanged(m_leftPage);
}

// ─────────────────────────────────────────────────────────────────────────────
// onPageReady
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::onPageReady(int page) {
  if (!m_cache)
    return;

  if (page == m_leftPage) {
    m_leftPx = m_cache->get(page);
    update();
  } else if (page == m_leftPage + 1) {
    m_rightPx = m_cache->get(page);
    update();
  } else {
    return; // página pré-cacheada fora do spread atual — ignora update visual
  }

  const bool rightDone =
      m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);
  if (m_leftPx && rightDone) {
    // Ambas prontas: cancela spinner e delay timer
    m_spinnerTimer->stop();
    m_spinnerDelayTimer->stop();
    m_showSpinner = false;
    update(); // garante repaint limpo sem spinner
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// API de texto — wrappers para PdfTextLayer
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::addHighlight(const HighlightEntry &h) {
  m_textLayer->addHighlight(h);
}
void CasualPdfView::removeHighlight(const QString &id) {
  m_textLayer->removeHighlight(id);
}
void CasualPdfView::clearHighlights() { m_textLayer->clearHighlights(); }
void CasualPdfView::setSearchHighlights(int p, const QList<QRectF> &r) {
  m_textLayer->setSearchHighlights(p, r);
}
void CasualPdfView::clearSearchHighlights() {
  m_textLayer->clearSearchHighlights();
}
void CasualPdfView::copyToClipboard() { m_textLayer->copyToClipboard(); }
void CasualPdfView::clearSelection() { m_textLayer->clearSelection(); }
void CasualPdfView::setSelectionMode(PdfTextLayer::SelectionMode m) {
  m_textLayer->setSelectionMode(m);
}
void CasualPdfView::setToolMode(PdfTextLayer::ToolMode mode) {
  m_textLayer->setToolMode(mode);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout helpers
// ─────────────────────────────────────────────────────────────────────────────
QRect CasualPdfView::leftPageRect() const {
  const int half = (width() - kGutterPx) / 2;
  return QRect(0, 0, half, height() - kFooterHeight);
}

QRect CasualPdfView::rightPageRect() const {
  const int half = (width() - kGutterPx) / 2;
  return QRect(half + kGutterPx, 0, half, height() - kFooterHeight);
}

// ─────────────────────────────────────────────────────────────────────────────
// pageRectForIndex — rect em pixels do widget para a página i.
//
// Retorna {} para páginas fora do spread atual (invisíveis).
// Usado como PageRectFn pela PdfTextLayer.
//
// A escala efetiva não é kCasualDpi/72 diretamente: a página é escalada para
// caber na metade disponível do widget mantendo a proporção PDF.  A escala
// real é fittedWidth / pageWidthPts (calculada em scaleForPage).
// ─────────────────────────────────────────────────────────────────────────────
QRect CasualPdfView::pageRectForIndex(int i) const {
  if (i != m_leftPage && i != m_leftPage + 1)
    return {};
  if (i < 0 || i >= m_pageSizes.size())
    return {};

  const QRect area = (i == m_leftPage) ? leftPageRect() : rightPageRect();
  const QRect inner = area.adjusted(kPageMarginPx, kPageMarginPx,
                                    -kPageMarginPx, -kPageMarginPx);

  // Escala para caber mantendo a proporção da página PDF
  const QSizeF pageSize = m_pageSizes[i];
  const QSizeF fitted = pageSize.scaled(inner.size(), Qt::KeepAspectRatio);

  const int x = inner.x() + (inner.width() - (int)fitted.width()) / 2;
  const int y = inner.y() + (inner.height() - (int)fitted.height()) / 2;
  return QRect(QPoint(x, y), fitted.toSize());
}

// ─────────────────────────────────────────────────────────────────────────────
// scaleForPage — razão pixels/pt da página i no spread atual.
//
// Difere de kCasualDpi/72 porque a página é escalonada para caber na metade
// do widget (fit-to-area), o que pode dar uma escala maior ou menor que
// kCasualDpi. A camada de texto usa esse valor para converter PDF pts ↔ pixels
// do widget.
// ─────────────────────────────────────────────────────────────────────────────
qreal CasualPdfView::scaleForPage(int i) const {
  const QRect r = pageRectForIndex(i);
  if (!r.isValid())
    return kCasualDpi / 72.0; // fallback seguro
  const QSizeF ps = m_pageSizes.value(i, QSizeF(595.0, 842.0));
  if (ps.width() <= 0)
    return kCasualDpi / 72.0;
  return static_cast<qreal>(r.width()) / ps.width();
}

// ─────────────────────────────────────────────────────────────────────────────
// drawSpinner — arco contínuo girante (estilo iOS / Material) centrado na área
void CasualPdfView::drawSpinner(QPainter &p, const QRect &area) const {
  const QPoint center = area.center();
  const int r = 16;

  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(center);

  // Arco de fundo (pista) — translúcido e fino
  QPen trackPen(QColor(160, 160, 160, 35), 2.5, Qt::SolidLine, Qt::RoundCap);
  p.setPen(trackPen);
  p.drawEllipse(QPoint(0, 0), r, r);

  // Arco animado — 150° com gradiente fade nas pontas
  const int arcLen = 150 * 16; // unidades de 1/16 de grau
  const int startAngle = (90 - m_spinnerAngle) * 16;

  // Gradiente conical: transparente → cor → opaco no final
  QConicalGradient grad(0, 0, 90 - m_spinnerAngle);
  grad.setColorAt(0.00, QColor(120, 120, 120, 0));
  grad.setColorAt(0.25, QColor(110, 110, 110, 140));
  grad.setColorAt(1.00, QColor(100, 100, 100, 230));

  QPen arcPen(QBrush(grad), 2.5, Qt::SolidLine, Qt::RoundCap);
  p.setPen(arcPen);
  p.drawArc(QRect(-r, -r, r * 2, r * 2), startAngle, arcLen);

  p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// paintEvent
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.setOpacity(m_opacity);

  // Fundo
  p.fillRect(rect(), m_bg);

  // Linha central (gutter)
  p.setPen(QPen(QColor(0, 0, 0, 22), 1));
  p.drawLine(width() / 2, kPageMarginPx / 2, width() / 2,
             height() - kPageMarginPx / 2);

  const auto drawPage = [&](const std::optional<QPixmap> &opt,
                            const QRect &area, bool showSpinner) {
    const QRect inner = area.adjusted(kPageMarginPx, kPageMarginPx,
                                      -kPageMarginPx, -kPageMarginPx);

    p.fillRect(inner.adjusted(kShadowPx, kShadowPx, kShadowPx, kShadowPx),
               QColor(0, 0, 0, 28));
    p.fillRect(inner, Qt::white);

    if (opt) {
      // Usa pageRectForIndex para garantir consistência com a camada de texto.
      // A diferença de 1-2px por arredondamento em relação a fitPixmap()
      // é imperceptível visualmente.
      const int idx = (&opt == &m_leftPx) ? m_leftPage : m_leftPage + 1;
      const QRect dest = pageRectForIndex(idx);
      if (dest.isValid())
        p.drawPixmap(dest, *opt);
      else
        p.drawPixmap(inner, *opt); // fallback se pageSizes não disponível
    } else if (showSpinner && m_showSpinner) {
      drawSpinner(p, inner);
    }

    p.setPen(QPen(QColor(0, 0, 0, 16), 1));
    p.drawRect(inner);
  };

  drawPage(m_leftPx, leftPageRect(), /*showSpinner=*/true);

  if (m_leftPage + 1 < m_pageCount) {
    drawPage(m_rightPx, rightPageRect(), /*showSpinner=*/true);
  } else {
    const QRect inner = rightPageRect().adjusted(
        kPageMarginPx, kPageMarginPx, -kPageMarginPx, -kPageMarginPx);
    p.fillRect(inner.adjusted(kShadowPx, kShadowPx, kShadowPx, kShadowPx),
               QColor(0, 0, 0, 16));
    p.fillRect(inner, Qt::white);
    p.setPen(QPen(QColor(0, 0, 0, 16), 1));
    p.drawRect(inner);
  }

  // ── Overlays de texto ─────────────────────────────────────────────────
  // A camada usa as mesmas pageRectForIndex/scaleForPage injetadas no ctor,
  // garantindo que highlights e seleção se sobreponham exatamente ao texto.
  m_textLayer->paintOverlays(p, rect());
}

// ─────────────────────────────────────────────────────────────────────────────
// showEvent / resizeEvent
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::showEvent(QShowEvent *e) {
  activateDpi();
  QWidget::showEvent(e);
  // Adia um frame para garantir que o widget já tem o tamanho final
  // atribuído pelo QStackedWidget antes de posicionar o footer.
  QTimer::singleShot(0, this, [this] {
    repositionFooter();
    if (m_footerWidget) {
      m_footerWidget->show();
      m_footerWidget->raise();
    }
  });
}

void CasualPdfView::resizeEvent(QResizeEvent *) {
  if (!m_leftPx || (!m_rightPx && m_leftPage + 1 < m_pageCount))
    requestCurrentPages();
  // pageRectForIndex recalcula a cada chamada usando o tamanho atual do widget
  repositionFooter();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse events — delegados integralmente à camada de texto.
// Navegação de páginas acontece apenas via teclado (← →) e roda do mouse.
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    (void)m_textLayer->handleMousePress(e);
    setFocus();
    e->accept();
  }
}

void CasualPdfView::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() & Qt::LeftButton) {
    (void)m_textLayer->handleMouseMove(e);
    e->accept();
  }
}

void CasualPdfView::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return;
  (void)m_textLayer->handleMouseRelease(e);
  e->accept();
}

void CasualPdfView::contextMenuEvent(QContextMenuEvent *e) {
  m_textLayer->handleContextMenu(e, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// keyPressEvent
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::keyPressEvent(QKeyEvent *e) {
  // Ctrl+C e Escape são consumidos pela camada de texto
  if (m_textLayer->handleKeyPress(e)) {
    e->accept();
    return;
  }

  switch (e->key()) {
  case Qt::Key_Right:
  case Qt::Key_PageDown:
  case Qt::Key_Space:
    nextSpread();
    break;
  case Qt::Key_Left:
  case Qt::Key_PageUp:
    prevSpread();
    break;
  default:
    QWidget::keyPressEvent(e);
    return;
  }
  e->accept();
}

void CasualPdfView::wheelEvent(QWheelEvent *e) {
  if (e->angleDelta().y() < 0)
    nextSpread();
  else if (e->angleDelta().y() > 0)
    prevSpread();
}

// ─────────────────────────────────────────────────────────────────────────────
// scheduleFadeIn
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::scheduleFadeIn() {
  // Se as páginas já estão em cache, exibe instantaneamente sem animação —
  // evita flash desnecessário em navegação de páginas pré-cacheadas.
  if (m_leftPx.has_value()) {
    m_fadeAnim->stop();
    m_opacity = 1.0;
    update();
    return;
  }
  m_fadeAnim->stop();
  m_opacity = 0.0;
  m_fadeAnim->setStartValue(0.0);
  m_fadeAnim->setEndValue(1.0);
  m_fadeAnim->start();
}
