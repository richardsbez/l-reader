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
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// mergeRectsByLine — une rects de caracteres/palavras em faixas de linha
//
// Algoritmo:
//   1. Ordena por Y (linha) e depois por X (posição na linha).
//      Threshold de 2 px absorve variações sub-pixel de diferentes renderers.
//   2. Agrupa rects cujo centro Y está dentro de 55 % da altura do rect
//      corrente — lida com letras ascendentes/descendentes na mesma linha.
//   3. Une horizontalmente (QRectF::united) → uma faixa por linha.
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
                                const QColor &color, qreal cornerRadius) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setPen(Qt::NoPen);
  p.setBrush(color);
  for (const QRectF &r : widgetRects) {
    // Altura mínima de 3 px para rects degenerados (espaços)
    const QRectF safe = r.height() < 3.0
                            ? r.adjusted(0, -(3.0 - r.height()) * 0.5, 0,
                                         (3.0 - r.height()) * 0.5)
                            : r;
    p.drawRoundedRect(safe, cornerRadius, cornerRadius);
  }
  p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// normalizeForClipboard — limpa o texto extraído do PDF para o clipboard
//
//   1. Remove soft-hyphens (U+00AD) — artefatos invisíveis em alguns editores.
//   2. Desfaz hifenização de linha/página ("pala-\nbra" → "palavra").
//   3. \n simples → espaço; \n\n (parágrafo real) → preservado.
//   4. Colapsa múltiplos espaços em um único.
//   5. Remove espaço residual antes de quebra de parágrafo.
// ─────────────────────────────────────────────────────────────────────────────
static QString normalizeForClipboard(const QString &raw) {
  QString s = raw;

  // 1. Soft-hyphens invisíveis
  s.remove(QChar(0x00AD));

  // 2. Hifenização de fim de linha/página (hífen normal U+002D e não-quebrável
  // U+2011)
  static const QRegularExpression reHyphen(
      QStringLiteral("[\\x{002D}\\x{2011}]\\n"),
      QRegularExpression::UseUnicodePropertiesOption);
  s.replace(reHyphen, QString());

  // 3. \n simples → espaço  |  \n\n (parágrafo) → preservado
  const QString kParaMark = QStringLiteral("\x00\x01");
  s.replace(QLatin1String("\n\n"), kParaMark);
  s.replace(QLatin1Char('\n'), QLatin1Char(' '));
  s.replace(kParaMark, QLatin1String("\n\n"));

  // 4. Múltiplos espaços → um único
  static const QRegularExpression reSpaces(QStringLiteral("  +"));
  s.replace(reSpaces, QStringLiteral(" "));

  // 5. Espaço residual antes de parágrafo
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
  // Throttle de 16 ms ≈ 60 fps para computeTextFlowSelection.
  // Sem isso, pg->textList() seria chamado a cada pixel arrastado.
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

    // Repinta apenas a união da área antiga + nova (dirty rect mínimo)
    const QRect dirty = oldRect.united(newRect);
    if (dirty.isEmpty())
      emit repaintAll();
    else
      emit repaintNeeded(dirty.adjusted(-4, -4, 4, 4));
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument / clear
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::setDocument(Poppler::Document *doc, int pageCount) {
  m_doc = doc;
  m_pageCount = pageCount;

  // Invalida o word cache do documento anterior.
  // Os rects Poppler pertencem ao documento — não podem ser reutilizados.
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
      // Prefere mergedPtRects (hit-testing mais preciso) quando disponíveis
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
// handleMousePress
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleMousePress(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton)
    return false;

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
    // Agenda processamento no próximo tick do throttle (16 ms)
    m_selPending = true;
    if (!m_selThrottle->isActive())
      m_selThrottle->start();
  } else {
    // RectMode: repinta apenas o rubber-band (dirty rect mínimo)
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

  // Modo Annotate: converte seleção em highlight permanente
  if (m_toolMode == ToolMode::Annotate && m_hasSelection &&
      !m_selectedText.isEmpty()) {
    HighlightEntry h;
    h.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    h.text = m_selectedText;
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
  }

  emit repaintAll();
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// handleKeyPress — consome apenas Ctrl+C e Escape
// ─────────────────────────────────────────────────────────────────────────────
bool PdfTextLayer::handleKeyPress(QKeyEvent *e) {
  switch (e->key()) {
  case Qt::Key_C:
    if (e->modifiers() & Qt::ControlModifier) {
      copyToClipboard();
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
// handleContextMenu
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::handleContextMenu(QContextMenuEvent *e, QWidget *viewport) {
  QMenu menu(viewport);

  const QString hlId = highlightIdAtPoint(e->pos());
  if (!hlId.isEmpty()) {
    auto *actDel = menu.addAction(
        QCoreApplication::translate("PdfTextLayer", "Excluir highlight"));
    QObject::connect(actDel, &QAction::triggered, this,
                     [this, hlId] { emit removeHighlightRequested(hlId); });
    menu.addSeparator();
  }

  if (m_hasSelection) {
    auto *actCopy =
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")),
                       QCoreApplication::translate(
                           "PdfTextLayer", "Copiar texto selecionado\tCtrl+C"));
    QObject::connect(actCopy, &QAction::triggered, this,
                     &PdfTextLayer::copyToClipboard);
    menu.addSeparator();
  }

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
// paintOverlays — pinta TODOS os overlays de texto em ordem correta:
//   1. Highlights permanentes (desenhados ANTES da seleção)
//   2. Highlights de busca (temporários, azul-ciano)
//   3. Seleção de texto (azul translúcido)
//   4. Rubber-band retangular (apenas durante drag em RectMode)
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

  // ── 4. Rubber-band retangular ─────────────────────────────────────────
  if (m_selecting && m_selMode == SelectionMode::RectMode) {
    const QRect selRect = QRect(m_selOrigin, m_selCurrent).normalized();
    if (!selRect.isEmpty()) {
      p.save();
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setPen(QPen(QColor(0x33, 0x99, 0xFF, 220), 1, Qt::DashLine));
      p.setBrush(QColor(0x33, 0x99, 0xFF, 30));
      p.drawRect(selRect);
      p.restore();
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// paintSelection — overlay azul da seleção de texto atual
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::paintSelection(QPainter &p, const QRect &clip) const {
  if (!m_hasSelection)
    return;

  static const QColor kSelColor(0x33, 0x99, 0xFF, 100);

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
    paintHighlightRects(p, widgetRects, kSelColor, kHlCornerRadius);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// getPageWords — retorna (e constrói se necessário) o cache de WordInfo
//
// Por que isso importa:
//   Poppler::Page::textList() percorre a árvore de texto e aloca centenas de
//   TextBox. Chamá-lo em todo mouseMoveEvent bloqueava o event loop por vários
//   ms. Com o cache, cada página é processada uma única vez por sessão.
//
//   Os rects estão em pontos PDF (invariantes de zoom/DPI), portanto o cache
//   é válido enquanto o mesmo documento estiver aberto.
//
// Retorno por valor (implicit-share copy, O(1)):
//   QCache pode eviccionar o item durante um insert() posterior; retornar
//   referência a item eviccionado seria UB.
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
// ensureMergedRects — preenche span.mergedPtRects se ainda estiver vazio.
// Chamado em addHighlight() e, como fallback, para highlights carregados
// do disco que ainda não têm o cache pré-calculado.
// ─────────────────────────────────────────────────────────────────────────────
void PdfTextLayer::ensureMergedRects(HighlightEntry &h) {
  for (auto &span : h.spans) {
    if (span.mergedPtRects.isEmpty() && !span.ptRects.isEmpty())
      span.mergedPtRects = mergeRectsByLine(span.ptRects);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// widgetPointToPagePt — converte ponto do widget em coordenadas de ponto PDF
//
// A lambda m_pageRectFn retorna {} para páginas fora da área visível,
// portanto a iteração completa é segura mesmo em documentos com centenas
// de páginas — a maioria retorna pr.isValid() == false imediatamente.
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
// selectionBoundingRectWidget — bounding rect da seleção em coords do widget.
// Usado para update(rect) parcial, evitando repaint completo.
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
// computeSelection — RectMode
// Extrai texto da área retangular selecionada via Poppler::Page::text().
// Chamado uma única vez no mouseRelease — não precisa de cache.
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
// computeTextFlowSelection — TextFlowMode
//
// Melhorias em relação à versão original de PdfCanvasView:
//   1. Word cache (getPageWords): pg->textList() chamado no máximo uma vez
//      por página por sessão — reduz de ~8 ms/frame para < 0,5 ms/frame.
//   2. Sem alocações de Page por frame: apenas QVector<QRectF> do cache.
//   3. Chamado pelo throttle timer, não diretamente em mouseMoveEvent.
//   4. m_pageRectFn retorna {} para páginas fora da área — loop seguro para
//      ambas as views (scroll contínuo e spread).
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

  // Fallback: clique acima de todas as páginas → começa da primeira visível
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
  // Fallback: clique abaixo de todas as páginas → termina na última visível
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
