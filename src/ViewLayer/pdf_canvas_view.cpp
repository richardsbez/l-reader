// pdf_canvas_view.cpp  —  l-reader

#include "pdf_canvas_view.hpp"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <climits>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
PdfCanvasView::PdfCanvasView(QWidget *parent) : QWidget(parent) {
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(0x18, 0x18, 0x18));
  setAutoFillBackground(true);
  setPalette(pal);
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::IBeamCursor);

  // ── Camada de texto ─────────────────────────────────────────────────────
  // As lambdas capturam `this` e sempre lêem o zoom atual — são avaliadas
  // a cada chamada, não apenas na construção.
  m_textLayer = std::make_unique<PdfTextLayer>(
      [this](int i) -> QRect { return pageRectInWidget(i); },
      [this](int /*i*/) -> qreal { return (BASE_DPI / 72.0) * m_zoom; }, this);

  // Repaint parcial ou total solicitado pela camada de texto
  connect(m_textLayer.get(), &PdfTextLayer::repaintAll, this,
          [this] { update(); });
  connect(m_textLayer.get(), &PdfTextLayer::repaintNeeded, this,
          [this](QRect r) { update(r); });

  // Cursor sugerido pela camada (muda com SelectionMode / ToolMode)
  connect(m_textLayer.get(), &PdfTextLayer::cursorChanged, this,
          [this](Qt::CursorShape s) { setCursor(s); });

  // Re-emite sinais da camada como sinais de PdfCanvasView
  connect(m_textLayer.get(), &PdfTextLayer::textSelected, this,
          &PdfCanvasView::textSelected);
  connect(m_textLayer.get(), &PdfTextLayer::highlightRequested, this,
          &PdfCanvasView::highlightRequested);
  connect(m_textLayer.get(), &PdfTextLayer::removeHighlightRequested, this,
          &PdfCanvasView::removeHighlightRequested);

  // Traduz enums da camada → enums públicos de PdfCanvasView
  connect(m_textLayer.get(), &PdfTextLayer::selectionModeChanged, this,
          [this](PdfTextLayer::SelectionMode m) {
            emit selectionModeChanged(
                m == PdfTextLayer::SelectionMode::TextFlowMode
                    ? SelectionMode::TextFlowMode
                    : SelectionMode::RectMode);
          });

  // ── Animação de zoom ────────────────────────────────────────────────────
  // Anima m_zoom suavemente. O easing OutCubic dá desaceleração natural.
  // Duração de 220 ms: responsiva sem parecer apressada.
  m_zoomAnim = new QVariantAnimation(this);
  m_zoomAnim->setDuration(220);
  m_zoomAnim->setEasingCurve(QEasingCurve::OutCubic);

  connect(
      m_zoomAnim, &QVariantAnimation::valueChanged, this,
      [this](const QVariant &v) {
        m_zoom = v.toReal();
        rebuildLayout(); // O(n) aritmética pura — usa m_pageSizes pré-carregado

        // Reposiciona a scrollbar para manter o ponto âncora fixo
        if (auto *sa = scrollArea()) {
          const int target =
              qRound(m_zoomAnchorFrac * m_totalHeight) - m_zoomAnchorScreenY;
          sa->verticalScrollBar()->setValue(
              std::max(sa->verticalScrollBar()->minimum(), target));
        }
        update();
        emit zoomChanged(m_zoom);
      });
}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument — inicializa o viewer com um novo documento
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setDocument(PageCache *cache, Poppler::Document *doc,
                                int pageCount) {
  m_cache = cache;
  m_doc = doc;
  m_pageCount = pageCount;
  m_currentPage = 0;
  m_zoom = 1.0;
  m_scrollY = 0;

  if (m_zoomAnim->state() == QAbstractAnimation::Running)
    m_zoomAnim->stop();
  m_zoomAnchorFrac = 0.0;
  m_zoomAnchorScreenY = 0;

  // ── Cache de tamanhos de página ─────────────────────────────────────────
  // Chamamos m_doc->page(i) aqui uma única vez por documento.
  // rebuildLayout() (chamado a cada frame de zoom) não precisa tocar no
  // Poppler.
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

  // Propaga o documento para a camada de texto (invalida word cache)
  m_textLayer->setDocument(doc, pageCount);

  rebuildLayout();
  update();
  emit currentPageChanged(0);
  emit zoomChanged(1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Wrappers públicos — delegam para m_textLayer
// ─────────────────────────────────────────────────────────────────────────────
PdfCanvasView::SelectionMode PdfCanvasView::selectionMode() const {
  return m_textLayer->selectionMode() ==
                 PdfTextLayer::SelectionMode::TextFlowMode
             ? SelectionMode::TextFlowMode
             : SelectionMode::RectMode;
}

void PdfCanvasView::setSelectionMode(SelectionMode m) {
  m_textLayer->setSelectionMode(m == SelectionMode::TextFlowMode
                                    ? PdfTextLayer::SelectionMode::TextFlowMode
                                    : PdfTextLayer::SelectionMode::RectMode);
}

void PdfCanvasView::setToolMode(ToolMode mode) {
  if (m_toolMode == mode)
    return;
  m_toolMode = mode;
  m_panning = false;

  if (mode == ToolMode::Pan) {
    // Pan é tratado diretamente nesta view; a camada de texto fica em Select
    setCursor(Qt::OpenHandCursor);
    m_textLayer->setToolMode(PdfTextLayer::ToolMode::Select);
  } else {
    m_textLayer->setToolMode(mode == ToolMode::Annotate
                                 ? PdfTextLayer::ToolMode::Annotate
                                 : PdfTextLayer::ToolMode::Select);
  }
  emit toolModeChanged(mode);
}

void PdfCanvasView::toggleSelectionMode() {
  m_textLayer->toggleSelectionMode();
}
void PdfCanvasView::copyToClipboard() { m_textLayer->copyToClipboard(); }
void PdfCanvasView::clearSelection() { m_textLayer->clearSelection(); }
void PdfCanvasView::addHighlight(const HighlightEntry &h) {
  m_textLayer->addHighlight(h);
}
void PdfCanvasView::removeHighlight(const QString &id) {
  m_textLayer->removeHighlight(id);
}
void PdfCanvasView::clearHighlights() { m_textLayer->clearHighlights(); }
QString PdfCanvasView::highlightIdAtPoint(const QPoint &p) const {
  return m_textLayer->highlightIdAtPoint(p);
}
void PdfCanvasView::setSearchHighlights(int page, const QList<QRectF> &r) {
  m_textLayer->setSearchHighlights(page, r);
}
void PdfCanvasView::clearSearchHighlights() {
  m_textLayer->clearSearchHighlights();
}
void PdfCanvasView::setHighlightColor(const QColor &color) {
  m_textLayer->setHighlightColor(color);
}
QColor PdfCanvasView::highlightColor() const {
  return m_textLayer->highlightColor();
}

// ─────────────────────────────────────────────────────────────────────────────
// goToPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::goToPage(int page) {
  if (m_pageCount == 0 || m_layout.isEmpty())
    return;
  page = std::clamp(page, 0, m_pageCount - 1);
  if (auto *sa = scrollArea())
    sa->verticalScrollBar()->setValue(m_layout[page].top - PAGE_GAP);
}

// ─────────────────────────────────────────────────────────────────────────────
// onScrollChanged / syncCurrentPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::onScrollChanged(int scrollY) {
  m_scrollY = scrollY;
  syncCurrentPage(scrollY);
}

void PdfCanvasView::syncCurrentPage(int scrollY) {
  if (m_layout.isEmpty())
    return;

  const int vpH = viewportHeight();
  const int vpCenter = scrollY + vpH / 2;
  int best = 0, bestDist = INT_MAX;

  for (int i = 0; i < m_layout.size(); ++i) {
    const int pageCenter = m_layout[i].top + m_layout[i].height / 2;
    const int dist = std::abs(pageCenter - vpCenter);
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }

  if (best != m_currentPage) {
    m_currentPage = best;
    emit currentPageChanged(m_currentPage);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// requestRepaintPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::requestRepaintPage(int page) {
  if (page < 0 || page >= m_layout.size())
    return;
  const auto &e = m_layout[page];
  const int vpH = viewportHeight();
  if (e.top <= m_scrollY + vpH && e.top + e.height >= m_scrollY)
    update(0, e.top, width(), e.height);
}

// ─────────────────────────────────────────────────────────────────────────────
// Zoom
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::zoomIn() { applyZoom(m_zoom + ZOOM_STEP); }
void PdfCanvasView::zoomOut() { applyZoom(m_zoom - ZOOM_STEP); }
void PdfCanvasView::zoomReset() { applyZoom(1.0); }

void PdfCanvasView::applyZoom(qreal newZoom) {
  zoomAround(newZoom, viewportHeight() / 2, /*animate=*/true);
}

void PdfCanvasView::zoomAround(qreal newZoom, int viewportAnchorY) {
  zoomAround(newZoom, viewportAnchorY, /*animate=*/true);
}

void PdfCanvasView::applyZoomFactor(qreal factor, int viewportAnchorY) {
  const qreal base = (m_zoomAnim->state() == QAbstractAnimation::Running)
                         ? m_zoomAnim->endValue().toReal()
                         : m_zoom;
  const qreal target = std::clamp(base * factor, ZOOM_MIN, ZOOM_MAX);
  zoomAround(target, viewportAnchorY, /*animate=*/true);
}

void PdfCanvasView::zoomAround(qreal newZoom, int viewportAnchorY,
                               bool animate) {
  newZoom = std::clamp(newZoom, ZOOM_MIN, ZOOM_MAX);

  const qreal targetZoom = (m_zoomAnim->state() == QAbstractAnimation::Running)
                               ? m_zoomAnim->endValue().toReal()
                               : m_zoom;
  if (qFuzzyCompare(newZoom, targetZoom))
    return;

  int currentScroll = m_scrollY;
  if (auto *sa = scrollArea())
    currentScroll = sa->verticalScrollBar()->value();

  m_zoomAnchorFrac = (m_totalHeight > 0)
                         ? static_cast<qreal>(currentScroll + viewportAnchorY) /
                               static_cast<qreal>(m_totalHeight)
                         : 0.0;
  m_zoomAnchorScreenY = viewportAnchorY;

  if (!animate) {
    if (m_zoomAnim->state() == QAbstractAnimation::Running)
      m_zoomAnim->stop();
    m_zoom = newZoom;
    rebuildLayout();
    if (auto *sa = scrollArea()) {
      const int t =
          qRound(m_zoomAnchorFrac * m_totalHeight) - m_zoomAnchorScreenY;
      sa->verticalScrollBar()->setValue(std::max(0, t));
    }
    update();
    emit zoomChanged(m_zoom);
    return;
  }

  if (m_zoomAnim->state() == QAbstractAnimation::Running)
    m_zoomAnim->stop();
  m_zoomAnim->setStartValue(m_zoom);
  m_zoomAnim->setEndValue(newZoom);
  m_zoomAnim->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildLayout — O(n) pura aritmética sobre m_pageSizes
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::rebuildLayout() {
  m_layout.clear();
  m_layout.reserve(m_pageCount);

  const qreal scale = (BASE_DPI / 72.0) * m_zoom;
  int y = PAGE_GAP;
  for (int i = 0; i < m_pageCount; ++i) {
    const QSizeF ptSize =
        (i < m_pageSizes.size()) ? m_pageSizes[i] : QSizeF(595.0, 842.0);
    const int w = qRound(ptSize.width() * scale);
    const int h = qRound(ptSize.height() * scale);
    m_layout.append({y, w, h});
    y += h + PAGE_GAP;
  }
  m_totalHeight = y;
  setMinimumSize(0, m_totalHeight);
  updateGeometry();
}

// ─────────────────────────────────────────────────────────────────────────────
// pageRectInWidget
// ─────────────────────────────────────────────────────────────────────────────
QRect PdfCanvasView::pageRectInWidget(int i) const {
  if (i < 0 || i >= m_layout.size())
    return {};
  const auto &e = m_layout[i];
  const int x = std::max(0, (width() - e.width) / 2);
  return QRect(x, e.top, e.width, e.height);
}

QSize PdfCanvasView::sizeHint() const {
  return QSize(800, m_totalHeight > 0 ? m_totalHeight : 600);
}

// ─────────────────────────────────────────────────────────────────────────────
// paintEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (!m_cache || m_pageCount == 0 || m_layout.isEmpty()) {
    p.setPen(QColor(0xaa, 0xaa, 0xaa));
    QFont f = p.font();
    f.setPointSize(15);
    p.setFont(f);
    p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap,
               QStringLiteral("Abra um documento para começar"
                              "\n\nPDF · EPUB · Markdown\nCtrl+O"));
    return;
  }

  const QRect clip = event->rect();
  const int viewW = width();

  // ── Páginas ───────────────────────────────────────────────────────────
  for (int i = 0; i < m_pageCount; ++i) {
    const auto &e = m_layout[i];
    if (e.top + e.height < clip.top())
      continue;
    if (e.top > clip.bottom())
      break;

    const int x = std::max(0, (viewW - e.width) / 2);
    const QRect pageRect(x, e.top, e.width, e.height);

    // Sombra multi-camada
    p.fillRect(pageRect.adjusted(-3, -1, 3, 3).translated(0, 8),
               QColor(0, 0, 0, 15));
    p.fillRect(pageRect.adjusted(-2, 0, 2, 2).translated(0, 6),
               QColor(0, 0, 0, 25));
    p.fillRect(pageRect.adjusted(-1, 0, 1, 1).translated(0, 4),
               QColor(0, 0, 0, 45));
    p.fillRect(pageRect.translated(0, 3), QColor(0, 0, 0, 60));

    const auto pixOpt = m_cache->get(i);
    if (pixOpt.has_value()) {
      p.drawPixmap(pageRect, pixOpt.value());
    } else {
      p.fillRect(pageRect, Qt::white);
      p.setPen(QColor(0x99, 0x99, 0x99));
      QFont f = p.font();
      f.setPointSize(11);
      p.setFont(f);
      p.drawText(pageRect, Qt::AlignCenter,
                 QStringLiteral("Carregando página %1…").arg(i + 1));
    }
  }

  // ── Overlays de texto (highlights, busca, seleção, rubber-band) ───────
  // paintOverlays() usa as mesmas lambdas de coordenadas que a camada
  // recebeu no construtor — sempre sincronizado com o zoom atual.
  m_textLayer->paintOverlays(p, clip);
}

// ─────────────────────────────────────────────────────────────────────────────
// keyPressEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::keyPressEvent(QKeyEvent *event) {
  // Ctrl+C e Escape são consumidos pela camada de texto
  if (m_textLayer->handleKeyPress(event)) {
    event->accept();
    return;
  }

  auto *sa = scrollArea();
  QScrollBar *vsb = sa ? sa->verticalScrollBar() : nullptr;

  const int lineStep = 48;
  const int pageStep = sa ? sa->viewport()->height() - lineStep : 600;

  switch (event->key()) {
  case Qt::Key_Down:
    if (vsb)
      vsb->setValue(vsb->value() + lineStep);
    break;
  case Qt::Key_Up:
    if (vsb)
      vsb->setValue(vsb->value() - lineStep);
    break;
  case Qt::Key_Space:
  case Qt::Key_PageDown:
    if (vsb)
      vsb->setValue(vsb->value() + pageStep);
    break;
  case Qt::Key_Backspace:
  case Qt::Key_PageUp:
    if (vsb)
      vsb->setValue(vsb->value() - pageStep);
    break;
  case Qt::Key_Left:
    goToPage(m_currentPage - 1);
    break;
  case Qt::Key_Right:
    goToPage(m_currentPage + 1);
    break;
  case Qt::Key_Home:
    goToPage(0);
    break;
  case Qt::Key_End:
    goToPage(m_pageCount - 1);
    break;
  case Qt::Key_Plus:
  case Qt::Key_Equal:
    zoomIn();
    break;
  case Qt::Key_Minus:
    zoomOut();
    break;
  case Qt::Key_0:
    if (event->modifiers() & Qt::ControlModifier)
      zoomReset();
    break;
  case Qt::Key_Tab:
    if (event->modifiers() == Qt::NoModifier)
      toggleSelectionMode();
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// resizeEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  update();
}

// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent — Pan tratado aqui; todo o resto vai para m_textLayer
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_toolMode == ToolMode::Pan) {
    m_panning = true;
    m_panOrigin = event->globalPosition().toPoint();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  if (m_textLayer->handleMousePress(event))
    event->accept();
  else
    QWidget::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseMoveEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mouseMoveEvent(QMouseEvent *event) {
  if (m_panning && (event->buttons() & Qt::LeftButton)) {
    const QPoint cur = event->globalPosition().toPoint();
    const QPoint delta = cur - m_panOrigin;
    m_panOrigin = cur;
    if (auto *sa = scrollArea()) {
      sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() -
                                          delta.x());
      sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() -
                                        delta.y());
    }
    event->accept();
    return;
  }
  if (m_textLayer->handleMouseMove(event))
    event->accept();
  else
    QWidget::mouseMoveEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseReleaseEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_panning) {
    m_panning = false;
    setCursor(Qt::OpenHandCursor);
    event->accept();
    return;
  }
  if (m_textLayer->handleMouseRelease(event))
    event->accept();
  else
    QWidget::mouseReleaseEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// contextMenuEvent — delegado para a camada de texto
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::contextMenuEvent(QContextMenuEvent *event) {
  m_textLayer->handleContextMenu(event, this);
}

// ─────────────────────────────────────────────────────────────────────────────
// wheelEvent — ignorado aqui; o scroll area pai processa a roda do mouse.
// A declaração ficou no .hpp sem corpo inline para evitar que QWheelEvent
// seja usado incompleto em translation units que incluem o header
// via main_window.hpp (onde QWheelEvent ainda é forward-declared).
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::wheelEvent(QWheelEvent *event) { event->ignore(); }
