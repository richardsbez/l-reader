// pdf_text_layer.cpp  —  l-reader

#include "pdf_text_layer.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// mergeRectsByLine — une rects de caracteres/palavras em faixas de linha
// ─────────────────────────────────────────────────────────────────────────────
static QVector<QRectF> mergeRectsByLine(const QVector<QRectF> &rects) {
  if (rects.isEmpty())
    return {};

  QVector<QRectF> sorted = rects;
  std::sort(sorted.begin(), sorted.end(), [](const QRectF &a, const QRectF &b) {
    const qreal dy = a.y() - b.y();
    if (qAbs(dy) > 2.0)
      return dy < 0;
    return a.x() < b.x();
  });

  QVector<QRectF> merged;
  merged.reserve(8);
  QRectF cur = sorted.first();
  for (int i = 1; i < sorted.size(); ++i) {
    const QRectF &r = sorted[i];
    if (qAbs(r.y() - cur.y()) <= cur.height() * 0.55)
      cur = cur.united(r);
    else {
      merged.append(cur);
      cur = r;
    }
  }
  merged.append(cur);
  return merged;
}

// ─────────────────────────────────────────────────────────────────────────────
// paintHighlightRects — faixas de highlight com cantos arredondados
// ─────────────────────────────────────────────────────────────────────────────
static void paintHighlightRects(QPainter &p, const QVector<QRectF> &widgetRects,
                                const QColor &fillColor, qreal cornerRadius,
                                const QColor &strokeColor = QColor(),
                                qreal strokeWidth = 0.0) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  for (const QRectF &r : widgetRects) {
    const QRectF safe = r.height() < 3.0
                            ? r.adjusted(0, -(3.0 - r.height()) * 0.5, 0,
                                         (3.0 - r.height()) * 0.5)
                            : r;

    // Fill
    p.setPen(Qt::NoPen);
    p.setBrush(fillColor);
    p.drawRoundedRect(safe, cornerRadius, cornerRadius);

    // Optional stroke (para seleção)
    if (strokeColor.isValid() && strokeWidth > 0.0) {
      p.setPen(QPen(strokeColor, strokeWidth));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(safe.adjusted(strokeWidth * 0.5, strokeWidth * 0.5,
                                      -strokeWidth * 0.5, -strokeWidth * 0.5),
                        cornerRadius, cornerRadius);
    }
  }
  p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// normalizeForClipboard
// ─────────────────────────────────────────────────────────────────────────────
static QString normalizeForClipboard(const QString &raw) {
  QString s = raw;
  s.remove(QChar(0x00AD));

  static const QRegularExpression reHyphen(
      QStringLiteral("[\\x{002D}\\x{2011}]\\n"),
      QRegularExpression::UseUnicodePropertiesOption);
  s.replace(reHyphen, QString());

  const QString kParaMark = QStringLiteral("\x00\x01");
  s.replace(QLatin1String("\n\n"), kParaMark);
  s.replace(QLatin1Char('\n'), QLatin1Char(' '));
  s.replace(kParaMark, QLatin1String("\n\n"));

  static const QRegularExpression reSpaces(QStringLiteral("  +"));
  s.replace(reSpaces, QStringLiteral(" "));

  static const QRegularExpression reTrail(QStringLiteral(" \\n\\n"));
  s.replace(reTrail, QStringLiteral("\n\n"));

  return s.trimmed();
}

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
PdfTextLayer::PdfTextLayer(PageRectFn pageRectFn, ScaleFn scaleFn,
                           QObject *parent)
    : QObject(parent), m_pageRectFn(std::move(pageRectFn)),
      m_scaleFn(std::move(scaleFn)) {

  // Throttle de seleção (16 ms ≈ 60 fps)
  m_selThrottle = new QTimer(this);
  m_selThrottle->setSingleShot(true);
  m_selThrottle->setInterval(16);
  connect(m_selThrottle, &QTimer::timeout, this, [this] {
    if (!m_selPending)
      return;
    m_selPending = false;
    const QRect oldRect = selectionBoundingRectWidget();
    computeTextFlowSelection();
    const QRect newRect = selectionBoundingRectWidget();
    const QRect dirty = oldRect.united(newRect);
    if (dirty.isEmpty())
      emit repaintAll();
    else
      emit repaintNeeded(dirty.adjusted(-4, -4, 4, 4));
  });

  // Timer de multi-clique — expira e reseta o contador de cliques
  m_clickTimer = new QTimer(this);
  m_clickTimer->setSingleShot(true);
  m_clickTimer->setInterval(kMultiClickMs);
  connect(m_clickTimer, &QTimer::timeout, this, [this] { m_clickCount = 0; });
}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument / clear
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::setDocument(Poppler::Document *doc, int pageCount) {
  m_doc = doc;
  m_pageCount = pageCount;
  m_wordCache.clear();
  clearSelection();
  clearSearchHighlights();
}

void PdfTextLayer::clear() {
  setDocument(nullptr, 0);
  clearHighlights();
}

// ─────────────────────────────────────────────────────────────────────────────
// Modos
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::setSelectionMode(SelectionMode m) {
  if (m == m_selMode)
    return;
  m_selMode = m;
  clearSelection();
  emit cursorChanged(m == SelectionMode::TextFlowMode ? Qt::IBeamCursor
                                                      : Qt::CrossCursor);
  emit selectionModeChanged(m_selMode);
}

void PdfTextLayer::setToolMode(ToolMode mode) {
  if (m_toolMode == mode)
    return;
  m_toolMode = mode;
  emit cursorChanged(suggestedCursor());
  emit toolModeChanged(m_toolMode);
}

void PdfTextLayer::toggleSelectionMode() {
  setSelectionMode(m_selMode == SelectionMode::TextFlowMode
                       ? SelectionMode::RectMode
                       : SelectionMode::TextFlowMode);
}

Qt::CursorShape PdfTextLayer::suggestedCursor() const noexcept {
  if (m_toolMode == ToolMode::Annotate)
    return Qt::IBeamCursor;
  return m_selMode == SelectionMode::TextFlowMode ? Qt::IBeamCursor
                                                  : Qt::CrossCursor;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cor ativa de highlight
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::setHighlightColor(const QColor &color) {
  if (m_activeHlColor == color)
    return;
  m_activeHlColor = color;
  emit highlightColorChanged(color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Seleção de texto
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::clearSelection() {
  m_selThrottle->stop();
  m_selPending = false;
  m_selecting = false;
  m_hasSelection = false;
  m_pageSelections.clear();
  m_selectedText.clear();
  emit repaintAll();
}

void PdfTextLayer::copyToClipboard() {
  if (!m_selectedText.isEmpty())
    QApplication::clipboard()->setText(normalizeForClipboard(m_selectedText));
}

// ─────────────────────────────────────────────────────────────────────────────
// Highlights permanentes
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::addHighlight(const HighlightEntry &h) {
  HighlightEntry entry = h;
  ensureMergedRects(entry);
  for (auto &existing : m_highlights) {
    if (existing.id == entry.id) {
      existing = entry;
      emit repaintAll();
      return;
    }
  }
  m_highlights.append(entry);
  emit repaintAll();
}

void PdfTextLayer::removeHighlight(const QString &id) {
  const int before = m_highlights.size();
  m_highlights.erase(
      std::remove_if(m_highlights.begin(), m_highlights.end(),
                     [&id](const HighlightEntry &h) { return h.id == id; }),
      m_highlights.end());
  if (m_highlights.size() != before)
    emit repaintAll();
}

void PdfTextLayer::clearHighlights() {
  if (m_highlights.isEmpty())
    return;
  m_highlights.clear();
  emit repaintAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// highlightIdAtPoint
// ─────────────────────────────────────────────────────────────────────────────
QString PdfTextLayer::highlightIdAtPoint(const QPoint &widgetPos) const {
  for (const auto &hl : std::as_const(m_highlights)) {
    for (const auto &span : hl.spans) {
      const QRect pr = m_pageRectFn(span.page);
      if (!pr.isValid())
        continue;
      const qreal scale = m_scaleFn(span.page);
      const QVector<QRectF> &rects =
          span.mergedPtRects.isEmpty() ? span.ptRects : span.mergedPtRects;
      for (const QRectF &ptR : rects) {
        const QRectF wr(ptR.x() * scale + pr.x(), ptR.y() * scale + pr.y(),
                        ptR.width() * scale, ptR.height() * scale);
        if (wr.contains(widgetPos))
          return hl.id;
      }
    }
  }
  return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Search highlights
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::setSearchHighlights(int page, const QList<QRectF> &ptRects) {
  m_searchHlPage = page;
  m_searchHlRects = ptRects;
  emit repaintAll();
}

void PdfTextLayer::clearSearchHighlights() {
  if (m_searchHlPage < 0 && m_searchHlRects.isEmpty())
    return;
  m_searchHlPage = -1;
  m_searchHlRects.clear();
  emit repaintAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// handleMousePress — inclui Shift+click e multi-click
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleMousePress(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return false;

  // ── Shift+click: estende a seleção existente ──────────────────────────
  if ((e->modifiers() & Qt::ShiftModifier) && (m_hasSelection || m_selecting)) {
    m_selCurrent = e->pos();
    if (m_selMode == SelectionMode::RectMode)
      computeSelection();
    else
      computeTextFlowSelection();
    emit repaintAll();
    return true;
  }

  // ── Rastreio de multi-clique ──────────────────────────────────────────
  const bool samePos =
      (e->pos() - m_lastClickPos).manhattanLength() <= kClickProximity;

  if (m_clickTimer->isActive() && samePos) {
    m_clickTimer->stop();
    m_clickCount++;
  } else {
    m_clickCount = 1;
  }
  m_lastClickPos = e->pos();
  m_clickTimer->start(kMultiClickMs);

  // ── Duplo-clique: seleciona palavra ───────────────────────────────────
  if (m_clickCount == 2) {
    selectWordAt(e->pos());
    return true;
  }
  // ── Triplo-clique: seleciona linha ────────────────────────────────────
  if (m_clickCount >= 3) {
    m_clickCount = 0;
    selectLineAt(e->pos());
    return true;
  }

  // ── Clique simples: inicia arrasto ────────────────────────────────────
  m_selThrottle->stop();
  m_selPending = false;
  m_selecting = true;
  m_hasSelection = false;
  m_selOrigin = e->pos();
  m_selCurrent = e->pos();
  m_pageSelections.clear();
  m_selectedText.clear();
  emit repaintAll();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleMouseMove
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleMouseMove(QMouseEvent *e) {
  if (!(e->buttons() & Qt::LeftButton) || !m_selecting)
    return false;

  m_selCurrent = e->pos();

  if (m_selMode == SelectionMode::TextFlowMode) {
    m_selPending = true;
    if (!m_selThrottle->isActive())
      m_selThrottle->start();
  } else {
    const QRect rubber = QRect(m_selOrigin, m_selCurrent).normalized();
    emit repaintNeeded(rubber.adjusted(-2, -2, 2, 2));
  }
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleMouseRelease
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleMouseRelease(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton || !m_selecting)
    return false;

  m_selThrottle->stop();
  m_selPending = false;
  m_selecting = false;
  m_selCurrent = e->pos();

  if (m_selMode == SelectionMode::RectMode)
    computeSelection();
  else
    computeTextFlowSelection();

  // Modo Annotate: cria highlight com a cor ativa ao soltar
  if (m_toolMode == ToolMode::Annotate && m_hasSelection &&
      !m_selectedText.isEmpty()) {
    createHighlightFromSelection(m_activeHlColor);
  }

  emit repaintAll();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleKeyPress
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleKeyPress(QKeyEvent *e) {
  switch (e->key()) {
  case Qt::Key_C:
    if (e->modifiers() & Qt::ControlModifier) {
      copyToClipboard();
      return true;
    }
    break;
  case Qt::Key_H:
    // Ctrl+H: cria highlight com a cor ativa
    if ((e->modifiers() & Qt::ControlModifier) && m_hasSelection) {
      createHighlightFromSelection(m_activeHlColor);
      return true;
    }
    break;
  case Qt::Key_Escape:
    if (m_hasSelection || m_selecting) {
      clearSelection();
      return true;
    }
    break;
  default:
    break;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleContextMenu — inclui submenu "Destacar como" com 4 cores
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::handleContextMenu(QContextMenuEvent *e, QWidget *viewport) {
  QMenu menu(viewport);

  // ── Excluir highlight existente ───────────────────────────────────────
  const QString hlId = highlightIdAtPoint(e->pos());
  if (!hlId.isEmpty()) {
    auto *actDel = menu.addAction(
        QCoreApplication::translate("PdfTextLayer", "Excluir highlight"));
    QObject::connect(actDel, &QAction::triggered, this,
                     [this, hlId] { emit removeHighlightRequested(hlId); });
    menu.addSeparator();
  }

  // ── Ações de seleção ─────────────────────────────────────────────────
  if (m_hasSelection) {
    // Copiar
    auto *actCopy =
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                       QCoreApplication::translate(
                           "PdfTextLayer", "Copiar texto selecionado\tCtrl+C"));
    QObject::connect(actCopy, &QAction::triggered, this,
                     &PdfTextLayer::copyToClipboard);

    // Submenu: Destacar como…
    auto *subHL = menu.addMenu(
        QCoreApplication::translate("PdfTextLayer", "Destacar como"));

    struct ColorOpt {
      const char *name;
      QColor color;
    };
    static const ColorOpt kColors[] = {
        {"Amarelo", QColor(255, 220, 0, 150)},
        {"Ciano", QColor(26, 196, 255, 150)},
        {"Verde", QColor(80, 210, 80, 150)},
        {"Rosa", QColor(255, 100, 180, 150)},
    };
    for (const auto &opt : kColors) {
      QPixmap px(14, 14);
      px.fill(QColor(0, 0, 0, 0));
      {
        QPainter pp(&px);
        pp.setRenderHint(QPainter::Antialiasing);
        pp.setPen(Qt::NoPen);
        pp.setBrush(opt.color);
        pp.drawRoundedRect(QRectF(1, 1, 12, 12), 3, 3);
      }
      const QColor c = opt.color;
      auto *act = subHL->addAction(
          QIcon(px), QCoreApplication::translate("PdfTextLayer", opt.name));
      QObject::connect(act, &QAction::triggered, this,
                       [this, c] { createHighlightFromSelection(c); });
    }

    menu.addSeparator();
  }

  // ── Alternar modo de seleção ──────────────────────────────────────────
  const bool isFlow = (m_selMode == SelectionMode::TextFlowMode);
  auto *actToggle = menu.addAction(
      isFlow ? QCoreApplication::translate("PdfTextLayer",
                                           "Mudar para: Seleção retangular")
             : QCoreApplication::translate(
                   "PdfTextLayer", "Mudar para: Seleção por fluxo de texto"));
  QObject::connect(actToggle, &QAction::triggered, this,
                   &PdfTextLayer::toggleSelectionMode);

  menu.exec(e->globalPos());
  e->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// paintOverlays
//   1. Highlights permanentes
//   2. Highlights de busca
//   3. Seleção de texto
//   4. Rubber-band retangular (RectMode)
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::paintOverlays(QPainter &p, const QRect &clip) const {
  // ── 1. Highlights permanentes ─────────────────────────────────────────
  for (const auto &hl : m_highlights) {
    for (const auto &span : hl.spans) {
      const QRect pr = m_pageRectFn(span.page);
      if (!pr.isValid() || !pr.intersects(clip))
        continue;
      const qreal scale = m_scaleFn(span.page);
      QVector<QRectF> widgetRects;
      widgetRects.reserve(span.mergedPtRects.size());
      for (const QRectF &ptR : span.mergedPtRects) {
        widgetRects.append(QRectF(ptR.x() * scale + pr.x(),
                                  ptR.y() * scale + pr.y(), ptR.width() * scale,
                                  ptR.height() * scale));
      }
      paintHighlightRects(p, widgetRects, hl.color, kHlCornerRadius);
    }
  }

  // ── 2. Highlights de busca ────────────────────────────────────────────
  if (m_searchHlPage >= 0 && !m_searchHlRects.isEmpty()) {
    const QRect pr = m_pageRectFn(m_searchHlPage);
    if (pr.isValid() && pr.intersects(clip)) {
      const qreal scale = m_scaleFn(m_searchHlPage);
      static const QColor kSearchColor(0x1A, 0xC4, 0xFF, 120);
      QVector<QRectF> widgetRects;
      widgetRects.reserve(m_searchHlRects.size());
      for (const QRectF &r : m_searchHlRects) {
        widgetRects.append(QRectF(r.x() * scale + pr.x(),
                                  r.y() * scale + pr.y(), r.width() * scale,
                                  r.height() * scale));
      }
      paintHighlightRects(p, widgetRects, kSearchColor, kHlCornerRadius);
    }
  }

  // ── 3. Seleção de texto ───────────────────────────────────────────────
  paintSelection(p, clip);

  // ── 4. Rubber-band retangular (polido) ────────────────────────────────
  if (m_selecting && m_selMode == SelectionMode::RectMode) {
    const QRect selRect = QRect(m_selOrigin, m_selCurrent).normalized();
    if (!selRect.isEmpty()) {
      p.save();
      p.setRenderHint(QPainter::Antialiasing, true);
      // Preenchimento translúcido
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0x33, 0x99, 0xFF, 35));
      p.drawRoundedRect(selRect, 3, 3);
      // Borda sólida
      p.setPen(QPen(QColor(0x33, 0x99, 0xFF, 210), 1.5, Qt::SolidLine));
      p.setBrush(Qt::NoBrush);
      p.drawRoundedRect(selRect.adjusted(1, 1, -1, -1), 3, 3);
      p.restore();
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// paintSelection — overlay azul da seleção, com borda sutil para legibilidade
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::paintSelection(QPainter &p, const QRect &clip) const {
  if (!m_hasSelection)
    return;

  static const QColor kSelFill(0x33, 0x99, 0xFF, 85);
  static const QColor kSelStroke(0x33, 0x99, 0xFF, 170);

  for (const auto &ps : m_pageSelections) {
    const QRect pr = m_pageRectFn(ps.page);
    if (!pr.isValid() || !pr.intersects(clip))
      continue;
    const qreal scale = m_scaleFn(ps.page);

    QVector<QRectF> widgetRects;
    widgetRects.reserve(ps.ptRects.size());
    for (const QRectF &ptR : mergeRectsByLine(ps.ptRects)) {
      widgetRects.append(QRectF(ptR.x() * scale + pr.x(),
                                ptR.y() * scale + pr.y(), ptR.width() * scale,
                                ptR.height() * scale));
    }
    paintHighlightRects(p, widgetRects, kSelFill, kHlCornerRadius, kSelStroke,
                        0.8);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// getPageWords
// ─────────────────────────────────────────────────────────────────────────────
QVector<PdfTextLayer::WordInfo> PdfTextLayer::getPageWords(int page) {
  if (auto *cached = m_wordCache.object(page))
    return *cached;

  QVector<WordInfo> words;
  if (m_doc) {
    if (auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(page))) {
      auto boxes = pg->textList();
      words.reserve(static_cast<int>(boxes.size()));
      for (const auto &tb : boxes) {
        WordInfo wi;
        wi.text = tb->text();
        wi.bbox = tb->boundingBox();
        wi.hasSpaceAfter = tb->hasSpaceAfter();
        const int n = wi.text.length();
        wi.charBoxes.reserve(n);
        for (int c = 0; c < n; ++c)
          wi.charBoxes.append(tb->charBoundingBox(c));
        words.append(std::move(wi));
      }
    }
  }

  const int cost = static_cast<int>(words.size());
  m_wordCache.insert(page, new QVector<WordInfo>(words), std::max(cost, 1));
  return words;
}

// ─────────────────────────────────────────────────────────────────────────────
// ensureMergedRects
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::ensureMergedRects(HighlightEntry &h) {
  for (auto &span : h.spans) {
    if (span.mergedPtRects.isEmpty() && !span.ptRects.isEmpty())
      span.mergedPtRects = mergeRectsByLine(span.ptRects);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// widgetPointToPagePt
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::widgetPointToPagePt(const QPoint &wp, int *outPage,
                                       qreal *outPtX, qreal *outPtY) const {
  for (int i = 0; i < m_pageCount; ++i) {
    const QRect pr = m_pageRectFn(i);
    if (!pr.isValid())
      continue;
    if (pr.contains(wp)) {
      const qreal scale = m_scaleFn(i);
      *outPage = i;
      *outPtX = (wp.x() - pr.x()) / scale;
      *outPtY = (wp.y() - pr.y()) / scale;
      return true;
    }
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// selectionBoundingRectWidget
// ─────────────────────────────────────────────────────────────────────────────
QRect PdfTextLayer::selectionBoundingRectWidget() const {
  if (m_pageSelections.isEmpty())
    return {};

  QRectF result;
  for (const auto &ps : m_pageSelections) {
    const QRect pr = m_pageRectFn(ps.page);
    if (!pr.isValid())
      continue;
    const qreal scale = m_scaleFn(ps.page);
    for (const QRectF &ptR : mergeRectsByLine(ps.ptRects)) {
      const QRectF wr(ptR.x() * scale + pr.x(), ptR.y() * scale + pr.y(),
                      ptR.width() * scale, ptR.height() * scale);
      result = result.isEmpty() ? wr : result.united(wr);
    }
  }
  return result.toRect().adjusted(-4, -4, 4, 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// selectWordAt — duplo-clique: seleciona a palavra sob o cursor
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::selectWordAt(const QPoint &widgetPos) {
  int page = -1;
  qreal ptX = 0, ptY = 0;
  if (!widgetPointToPagePt(widgetPos, &page, &ptX, &ptY))
    return;

  const auto words = getPageWords(page);
  for (const WordInfo &wi : words) {
    // Verifica bbox expandida levemente para facilitar o hit em bordas
    if (!wi.bbox.adjusted(-1, -1, 1, 1).contains(ptX, ptY))
      continue;

    QVector<QRectF> rects;
    rects.reserve(wi.charBoxes.size());
    for (const QRectF &cb : wi.charBoxes)
      if (!cb.isEmpty())
        rects.append(cb);
    if (rects.isEmpty())
      rects.append(wi.bbox);

    m_pageSelections.clear();
    m_pageSelections.append({page, rects});
    m_selectedText = wi.text;
    m_hasSelection = true;
    m_selecting = false;
    emit textSelected(m_selectedText);
    emit repaintAll();
    return;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// selectLineAt — triplo-clique: seleciona a linha inteira sob o cursor
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::selectLineAt(const QPoint &widgetPos) {
  int page = -1;
  qreal ptX = 0, ptY = 0;
  if (!widgetPointToPagePt(widgetPos, &page, &ptX, &ptY))
    return;

  const auto words = getPageWords(page);

  // Encontra o Y central da linha clicada
  qreal lineY = -1, lineH = 0;
  for (const WordInfo &wi : words) {
    if (wi.bbox.adjusted(-1, -1, 1, 1).contains(ptX, ptY)) {
      lineY = wi.bbox.center().y();
      lineH = wi.bbox.height();
      break;
    }
  }
  if (lineY < 0)
    return;

  // Coleta todas as palavras nessa linha (mesma faixa Y)
  QVector<QRectF> rects;
  QString lineText;
  bool prevSpace = false;

  for (const WordInfo &wi : words) {
    if (qAbs(wi.bbox.center().y() - lineY) > lineH * 0.60)
      continue;

    if (!lineText.isEmpty() && prevSpace)
      lineText += QLatin1Char(' ');

    for (const QRectF &cb : wi.charBoxes)
      if (!cb.isEmpty())
        rects.append(cb);
    if (rects.isEmpty())
      rects.append(wi.bbox);

    lineText += wi.text;
    prevSpace = wi.hasSpaceAfter;
  }

  if (rects.isEmpty())
    return;

  m_pageSelections.clear();
  m_pageSelections.append({page, rects});
  m_selectedText = lineText;
  m_hasSelection = true;
  m_selecting = false;
  emit textSelected(m_selectedText);
  emit repaintAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// createHighlightFromSelection — emite highlightRequested e limpa a seleção
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::createHighlightFromSelection(const QColor &color) {
  if (!m_hasSelection || m_selectedText.isEmpty())
    return;

  HighlightEntry h;
  h.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
  h.text = m_selectedText;
  h.color = color;
  for (const auto &ps : std::as_const(m_pageSelections)) {
    HighlightPageSpan span;
    span.page = ps.page;
    span.ptRects = ps.ptRects;
    h.spans.append(span);
  }
  if (!h.spans.isEmpty())
    emit highlightRequested(h);

  // Limpa seleção visual após criar o highlight
  m_hasSelection = false;
  m_pageSelections.clear();
  m_selectedText.clear();
  emit repaintAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// computeSelection — RectMode
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::computeSelection() {
  m_pageSelections.clear();
  m_selectedText.clear();
  m_hasSelection = false;

  if (!m_doc)
    return;

  const QRect sel = QRect(m_selOrigin, m_selCurrent).normalized();
  if (sel.isEmpty())
    return;

  for (int i = 0; i < m_pageCount; ++i) {
    const QRect pr = m_pageRectFn(i);
    if (!pr.isValid())
      continue;
    const QRect inter = pr.intersected(sel);
    if (inter.isEmpty())
      continue;

    const qreal scale = m_scaleFn(i);
    const QRect local(inter.x() - pr.x(), inter.y() - pr.y(), inter.width(),
                      inter.height());
    const QRectF ptRect(local.x() / scale, local.y() / scale,
                        local.width() / scale, local.height() / scale);

    auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
    if (!pg)
      continue;

    const QString pageText = pg->text(ptRect);
    if (pageText.trimmed().isEmpty())
      continue;

    m_pageSelections.append({i, {ptRect}});
    if (!m_selectedText.isEmpty())
      m_selectedText += QLatin1Char('\n');
    m_selectedText += pageText;
  }

  m_hasSelection = !m_selectedText.isEmpty();
  if (m_hasSelection)
    emit textSelected(m_selectedText);
}

// ─────────────────────────────────────────────────────────────────────────────
// computeTextFlowSelection — TextFlowMode (chamado pelo throttle)
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::computeTextFlowSelection() {
  m_pageSelections.clear();
  m_selectedText.clear();
  m_hasSelection = false;

  if (!m_doc)
    return;

  QPoint topWidget = m_selOrigin;
  QPoint botWidget = m_selCurrent;
  if (topWidget.y() > botWidget.y())
    std::swap(topWidget, botWidget);

  int startPage = -1, endPage = -1;
  qreal startPtX = 0, startPtY = 0, endPtX = 0, endPtY = 0;

  widgetPointToPagePt(topWidget, &startPage, &startPtX, &startPtY);
  widgetPointToPagePt(botWidget, &endPage, &endPtX, &endPtY);

  if (startPage < 0) {
    for (int i = 0; i < m_pageCount; ++i) {
      const QRect pr = m_pageRectFn(i);
      if (pr.isValid() && pr.top() >= topWidget.y()) {
        startPage = i;
        startPtX = 0;
        startPtY = 0;
        break;
      }
    }
    if (startPage < 0)
      startPage = 0;
  }
  if (endPage < 0) {
    for (int i = m_pageCount - 1; i >= 0; --i) {
      const QRect pr = m_pageRectFn(i);
      if (!pr.isValid())
        continue;
      if (pr.bottom() <= botWidget.y()) {
        const auto words = getPageWords(i);
        endPage = i;
        if (!words.isEmpty()) {
          endPtX = words.last().bbox.right();
          endPtY = words.last().bbox.bottom();
        } else {
          auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
          const QSizeF sz = pg ? pg->pageSizeF() : QSizeF(595, 842);
          endPtX = sz.width();
          endPtY = sz.height();
        }
        break;
      }
    }
    if (endPage < 0)
      endPage = m_pageCount - 1;
  }
  if (startPage > endPage)
    std::swap(startPage, endPage);

  for (int i = startPage; i <= endPage; ++i) {
    const QVector<WordInfo> words = getPageWords(i);
    if (words.isEmpty())
      continue;

    const bool isFirst = (i == startPage);
    const bool isLast = (i == endPage);
    const bool isSingle = isFirst && isLast;

    QVector<QRectF> rects;
    QString pageText;
    bool prevHadSpace = false;

    for (const WordInfo &wi : words) {
      const qreal cy = wi.bbox.center().y();
      const qreal hh = wi.bbox.height() * 0.6;
      const int nChars = wi.text.length();
      if (nChars == 0)
        continue;

      bool wordAddedAnyChar = false;

      for (int c = 0; c < nChars; ++c) {
        const QRectF &charBox = wi.charBoxes.value(c);
        if (charBox.isEmpty())
          continue;

        const qreal ccx = charBox.center().x();
        bool charIn = false;

        if (isSingle) {
          const bool onStart = qAbs(cy - startPtY) <= hh;
          const bool onEnd = qAbs(cy - endPtY) <= hh;
          const bool betweenY = (cy > startPtY + hh) && (cy < endPtY - hh);

          if (betweenY) {
            charIn = true;
          } else if (onStart && onEnd) {
            const qreal xMin = qMin(startPtX, endPtX);
            const qreal xMax = qMax(startPtX, endPtX);
            charIn = (ccx >= xMin && ccx <= xMax);
          } else if (onStart) {
            charIn = (ccx >= startPtX);
          } else if (onEnd) {
            charIn = (ccx <= endPtX);
          }
        } else if (isFirst) {
          const bool onStartLine = qAbs(cy - startPtY) <= hh;
          charIn = onStartLine ? (ccx >= startPtX) : (cy > startPtY - hh);
        } else if (isLast) {
          const bool onEndLine = qAbs(cy - endPtY) <= hh;
          charIn = onEndLine ? (ccx <= endPtX) : (cy < endPtY + hh);
        } else {
          charIn = true;
        }

        if (charIn) {
          if (c == 0 && !pageText.isEmpty() && prevHadSpace)
            pageText += QLatin1Char(' ');
          rects.append(charBox);
          pageText += wi.text[c];
          wordAddedAnyChar = true;
        }
      }
      if (wordAddedAnyChar)
        prevHadSpace = wi.hasSpaceAfter;
    }

    if (!rects.isEmpty()) {
      m_pageSelections.append({i, rects});
      if (!m_selectedText.isEmpty())
        m_selectedText += QLatin1Char('\n');
      m_selectedText += pageText;
    }
  }

  m_hasSelection = !m_selectedText.isEmpty();
  if (!m_selecting && m_hasSelection)
    emit textSelected(m_selectedText);
}
