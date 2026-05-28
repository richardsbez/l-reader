// casual_pdf_view.cpp  —  l-reader

#include "casual_pdf_view.hpp"

#include <QContextMenuEvent>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
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
  m_fadeAnim = new QPropertyAnimation(this, "opacity", this);
  m_fadeAnim->setDuration(200);
  m_fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

  // ── Spinner de carregamento ─────────────────────────────────────────────
  m_spinnerTimer = new QTimer(this);
  m_spinnerTimer->setInterval(40); // 30° a cada 40 ms → 75 rpm
  connect(m_spinnerTimer, &QTimer::timeout, this, [this] {
    m_spinnerAngle = (m_spinnerAngle + 30) % 360;
    if (!m_leftPx || (!m_rightPx && m_leftPage + 1 < m_pageCount))
      update();
    else
      m_spinnerTimer->stop();
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

  m_spinnerAngle = 0;
  m_spinnerTimer->start();
  update();

  requestCurrentPages();
  prefetchAdjacentSpreads();
  scheduleFadeIn();
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

  if (!m_leftPx)
    m_cache->requestRender(m_leftPage);
  if (!m_rightPx && m_leftPage + 1 < m_pageCount)
    m_cache->requestRender(m_leftPage + 1);

  if (m_leftPx && (m_rightPx || m_leftPage + 1 >= m_pageCount))
    m_spinnerTimer->stop();

  update();
}

void CasualPdfView::prefetchAdjacentSpreads() {
  if (!m_cache)
    return;
  const int next = m_leftPage + 2;
  const int prev = m_leftPage - 2;
  if (next < m_pageCount)
    m_cache->requestGalleryRender(next);
  if (next + 1 < m_pageCount)
    m_cache->requestGalleryRender(next + 1);
  if (prev >= 0)
    m_cache->requestGalleryRender(prev);
  if (prev + 1 >= 0)
    m_cache->requestGalleryRender(prev + 1);
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
  }

  const bool rightDone =
      m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);
  if (m_leftPx && rightDone)
    m_spinnerTimer->stop();
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
  return QRect(0, 0, half, height());
}

QRect CasualPdfView::rightPageRect() const {
  const int half = (width() - kGutterPx) / 2;
  return QRect(half + kGutterPx, 0, half, height());
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
// drawSpinner — arco girante no centro da área de página (estilo iOS)
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::drawSpinner(QPainter &p, const QRect &area) const {
  const QPoint center = area.center();
  const int r = 14;

  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(center);
  p.rotate(m_spinnerAngle);

  for (int i = 0; i < 8; ++i) {
    const int alpha = 40 + (215 / 8) * i;
    p.setPen(QPen(QColor(0, 0, 0, alpha), 2.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(0, r / 2 + 2, 0, r);
    p.rotate(45);
  }
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
    } else if (showSpinner) {
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
}

void CasualPdfView::resizeEvent(QResizeEvent *) {
  if (!m_leftPx || (!m_rightPx && m_leftPage + 1 < m_pageCount))
    requestCurrentPages();
  // pageRectForIndex recalcula a cada chamada usando o tamanho atual do widget
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
  m_fadeAnim->stop();
  m_opacity = 0.0;
  m_fadeAnim->setStartValue(0.0);
  m_fadeAnim->setEndValue(1.0);
  m_fadeAnim->start();
}
