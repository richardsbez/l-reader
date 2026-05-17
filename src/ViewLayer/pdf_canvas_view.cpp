// pdf_canvas_view.cpp
#include "pdf_canvas_view.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QUuid>
#include <QRegularExpression>
#include <climits>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// mergeRectsByLine — une rects de caracteres/palavras em faixas de linha
//
// Algoritmo:
//   1. Ordena por Y (linha) e depois por X (posição na linha).
//      O critério de "mesmo Y" usa um threshold de 2 px para absorver
//      variações sub-pixel de PDFs gerados por diferentes renderizadores.
//   2. Agrupa rects cujo centro Y está dentro de 55 % da altura do rect
//      corrente — lida bem com letras ascendentes/descendentes que variam
//      a bounding-box vertical dentro da mesma linha tipográfica.
//   3. Une horizontalmente (QRectF::united) formando uma faixa por linha.
//
// Resultado: highlight contínuo da primeira à última palavra, exatamente
// como na seleção nativa de texto de leitores PDF modernos.
// ─────────────────────────────────────────────────────────────────────────────
static QVector<QRectF> mergeRectsByLine(const QVector<QRectF>& rects)
{
    if (rects.isEmpty()) return {};

    QVector<QRectF> sorted = rects;
    std::sort(sorted.begin(), sorted.end(), [](const QRectF& a, const QRectF& b) {
        const qreal dy = a.y() - b.y();
        if (qAbs(dy) > 2.0) return dy < 0;
        return a.x() < b.x();
    });

    QVector<QRectF> merged;
    merged.reserve(8);

    QRectF cur = sorted.first();
    for (int i = 1; i < sorted.size(); ++i) {
        const QRectF& r = sorted[i];
        if (qAbs(r.y() - cur.y()) <= cur.height() * 0.55) {
            cur = cur.united(r);
        } else {
            merged.append(cur);
            cur = r;
        }
    }
    merged.append(cur);
    return merged;
}

// ─────────────────────────────────────────────────────────────────────────────
// paintHighlightRects — desenha faixas de highlight com cantos arredondados
//
// Recebe rects já em coordenadas de widget (pixels de tela).
// Usa antialiasing + RenderHint::Antialiasing para suavizar as bordas.
// ─────────────────────────────────────────────────────────────────────────────
static void paintHighlightRects(QPainter& p,
                                const QVector<QRectF>& widgetRects,
                                const QColor& color,
                                qreal cornerRadius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    for (const QRectF& r : widgetRects) {
        // Altura mínima de 3 px para rects degenerados (p.ex. espaços)
        const QRectF safe = r.height() < 3.0
            ? r.adjusted(0, -(3.0 - r.height()) * 0.5, 0, (3.0 - r.height()) * 0.5)
            : r;
        p.drawRoundedRect(safe, cornerRadius, cornerRadius);
    }
    p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// Construtor — inicializa aparência e o throttle timer de seleção
// ─────────────────────────────────────────────────────────────────────────────
PdfCanvasView::PdfCanvasView(QWidget* parent)
    : QWidget(parent)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x18, 0x18, 0x18));
    setAutoFillBackground(true);
    setPalette(pal);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);

    // ── Throttle de 16 ms ≈ 60 fps para computeTextFlowSelection ──────────
    // Sem esse timer, o drag de seleção chamava pg->textList() a cada pixel
    // arrastado, bloqueando o event loop e travando a UI.
    m_selThrottle = new QTimer(this);
    m_selThrottle->setSingleShot(true);
    m_selThrottle->setInterval(16);   // ≈ 60 fps
    connect(m_selThrottle, &QTimer::timeout, this, [this]() {
        if (!m_selPending) return;
        m_selPending = false;

        // Guarda o bounding rect ANTES para também invalidar a área antiga
        const QRect oldRect = selectionBoundingRectWidget();

        computeTextFlowSelection();

        // Repinta apenas a união da área antiga + nova (evita full-widget update)
        const QRect newRect = selectionBoundingRectWidget();
        QRect dirty = oldRect.united(newRect);
        if (dirty.isEmpty())
            update();
        else
            update(dirty.adjusted(-4, -4, 4, 4));
    });

    // ── Zoom animation ──────────────────────────────────────────────────────
    // Anima m_zoom de forma suave entre dois níveis.
    // O easing OutCubic dá uma desaceleração natural: arranca rápido e freia.
    // Duração de 220 ms é responsiva sem parecer apressada.
    m_zoomAnim = new QVariantAnimation(this);
    m_zoomAnim->setDuration(220);
    m_zoomAnim->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_zoomAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_zoom = v.toReal();
        rebuildLayout();   // O(n) pura aritmética graças ao m_pageSizes cache

        // Reposiciona a scrollbar a cada frame para manter o ponto âncora fixo.
        // Fórmula: scroll_target = anchorFrac * totalHeight - anchorScreenY
        if (auto* sa = scrollArea()) {
            const int target = qRound(m_zoomAnchorFrac * m_totalHeight) - m_zoomAnchorScreenY;
            sa->verticalScrollBar()->setValue(std::max(sa->verticalScrollBar()->minimum(), target));
        }
        update();
        emit zoomChanged(m_zoom);   // MainWindow debounça o DPI — só redita ao parar
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument — inicializa o viewer com um novo documento
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setDocument(PageCache* cache, Poppler::Document* doc, int pageCount)
{
    m_cache       = cache;
    m_doc         = doc;
    m_pageCount   = pageCount;
    m_currentPage = 0;
    m_zoom        = 1.0;
    m_scrollY     = 0;

    // Para qualquer animação de zoom do documento anterior
    if (m_zoomAnim->state() == QAbstractAnimation::Running)
        m_zoomAnim->stop();
    m_zoomAnchorFrac    = 0.0;
    m_zoomAnchorScreenY = 0;

    // ── Cache de tamanhos de página ────────────────────────────────────────
    // Chamamos m_doc->page(i) aqui uma única vez por documento.
    // rebuildLayout() (chamada a cada frame de zoom animado) não precisa mais
    // tocar no Poppler — é apenas aritmética sobre m_pageSizes.
    m_pageSizes.clear();
    m_pageSizes.reserve(pageCount);
    for (int i = 0; i < pageCount; ++i) {
        QSizeF sz;
        if (doc) {
            if (auto pg = std::unique_ptr<Poppler::Page>(doc->page(i)))
                sz = pg->pageSizeF();
        }
        if (sz.isEmpty()) sz = QSizeF(595.0, 842.0);   // A4 como fallback
        m_pageSizes.append(sz);
    }

    // ── Invalida o cache de palavras do documento anterior ─────────────────
    // Os rects do Poppler são em pontos PDF e independem do zoom,
    // mas pertencem ao documento antigo — devem ser descartados.
    m_wordCache.clear();

    rebuildLayout();
    update();
    emit currentPageChanged(0);
    emit zoomChanged(1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// getPageWords — retorna (e constrói se necessário) o cache de palavras
//               de uma página.
//
// Por que isso importa:
//   Poppler::Page::textList() percorre a árvore de texto do PDF e aloca
//   centenas de objetos TextBox.  Chamar isso em todo mouseMoveEvent (até
//   ~200×/s em monitores 200 Hz) bloqueava o event loop por vários ms por
//   chamada.  Com o cache, cada página é processada uma única vez.
//
//   Os rects estão em coordenadas de pontos PDF (independentes do zoom),
//   então o cache é válido enquanto o mesmo documento estiver aberto.
//
// Retorno por valor (não por referência):
//   QCache pode eviccionar o item durante um insert() subsequente.
//   Retornar referência a um item já eviccionado seria undefined behavior.
//   QVector usa implicit sharing, por isso cópias são baratas (O(1)).
// ─────────────────────────────────────────────────────────────────────────────
QVector<PdfCanvasView::WordInfo> PdfCanvasView::getPageWords(int page)
{
    if (auto* cached = m_wordCache.object(page))
        return *cached;    // implicit-share copy, O(1)

    QVector<WordInfo> words;
    if (m_doc) {
        auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(page));
        if (pg) {
            auto boxes = pg->textList();
            words.reserve(static_cast<int>(boxes.size()));
            for (const auto& tb : boxes) {
                WordInfo wi;
                wi.text          = tb->text();
                wi.bbox          = tb->boundingBox();
                wi.hasSpaceAfter = tb->hasSpaceAfter();
                const int n = wi.text.length();
                wi.charBoxes.reserve(n);
                for (int c = 0; c < n; ++c)
                    wi.charBoxes.append(tb->charBoundingBox(c));
                words.append(std::move(wi));
            }
        }
    }

    const int cost = static_cast<int>(words.size()); // custo = nº de palavras
    m_wordCache.insert(page, new QVector<WordInfo>(words), std::max(cost, 1));
    return words;
}

// ─────────────────────────────────────────────────────────────────────────────
// ensureMergedRects — preenche span.mergedPtRects se ainda estiver vazio.
// Chamado em addHighlight() e, como fallback, em paintEvent para highlights
// carregados do disco sem o cache pré-calculado.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::ensureMergedRects(HighlightEntry& h)
{
    for (auto& span : h.spans) {
        if (span.mergedPtRects.isEmpty() && !span.ptRects.isEmpty())
            span.mergedPtRects = mergeRectsByLine(span.ptRects);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// selectionBoundingRectWidget — bounding rect (coords widget) da seleção atual.
// Usado para update(rect) parcial em mouseMoveEvent, evitando full repaint.
// ─────────────────────────────────────────────────────────────────────────────
QRect PdfCanvasView::selectionBoundingRectWidget() const
{
    if (m_pageSelections.isEmpty()) return {};

    const qreal scale = (BASE_DPI / 72.0) * m_zoom;
    QRectF result;

    for (const auto& ps : m_pageSelections) {
        const QRect pr = pageRectInWidget(ps.page);
        for (const QRectF& ptR : mergeRectsByLine(ps.ptRects)) {
            const QRectF wr(
                ptR.x()      * scale + pr.x(),
                ptR.y()      * scale + pr.y(),
                ptR.width()  * scale,
                ptR.height() * scale);
            result = result.isEmpty() ? wr : result.united(wr);
        }
    }
    // Margem extra para cobrir cantos arredondados e anti-alias
    return result.toRect().adjusted(-4, -4, 4, 4);
}

// ─────────────────────────────────────────────────────────────────────────────
// paintSelection — pinta o overlay azul de seleção de texto.
// Factorizado para evitar duplicação no paintEvent.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::paintSelection(QPainter& p, const QRect& clip) const
{
    if (!m_hasSelection) return;

    const qreal scale = (BASE_DPI / 72.0) * m_zoom;
    static const QColor kSelColor(0x33, 0x99, 0xFF, 100);

    for (const auto& ps : m_pageSelections) {
        const QRect pr = pageRectInWidget(ps.page);
        if (!pr.intersects(clip)) continue;

        QVector<QRectF> widgetRects;
        widgetRects.reserve(ps.ptRects.size());
        for (const QRectF& ptR : mergeRectsByLine(ps.ptRects)) {
            widgetRects.append(QRectF(
                ptR.x()      * scale + pr.x(),
                ptR.y()      * scale + pr.y(),
                ptR.width()  * scale,
                ptR.height() * scale));
        }
        paintHighlightRects(p, widgetRects, kSelColor, HL_CORNER_RADIUS);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// sizeHint
// ─────────────────────────────────────────────────────────────────────────────
QSize PdfCanvasView::sizeHint() const
{
    return QSize(800, m_totalHeight > 0 ? m_totalHeight : 600);
}

// ─────────────────────────────────────────────────────────────────────────────
// goToPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::goToPage(int page)
{
    if (m_pageCount == 0 || m_layout.isEmpty()) return;
    page = std::clamp(page, 0, m_pageCount - 1);
    if (auto* sa = scrollArea())
        sa->verticalScrollBar()->setValue(m_layout[page].top - PAGE_GAP);
}

// ─────────────────────────────────────────────────────────────────────────────
// onScrollChanged
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::onScrollChanged(int scrollY)
{
    m_scrollY = scrollY;
    syncCurrentPage(scrollY);
}

// ─────────────────────────────────────────────────────────────────────────────
// requestRepaintPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::requestRepaintPage(int page)
{
    if (page < 0 || page >= m_layout.size()) return;
    const auto& e = m_layout[page];
    const int vpH = viewportHeight();
    if (e.top <= m_scrollY + vpH && e.top + e.height >= m_scrollY)
        update(0, e.top, width(), e.height);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots de zoom
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::zoomIn()    { applyZoom(m_zoom + ZOOM_STEP); }
void PdfCanvasView::zoomOut()   { applyZoom(m_zoom - ZOOM_STEP); }
void PdfCanvasView::zoomReset() { applyZoom(1.0); }

// ─────────────────────────────────────────────────────────────────────────────
// paintEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::paintEvent(QPaintEvent* event)
{
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

    const QRect clip  = event->rect();
    const int   viewW = width();
    const qreal scale = (BASE_DPI / 72.0) * m_zoom;

    // ── Páginas ───────────────────────────────────────────────────────────
    for (int i = 0; i < m_pageCount; ++i) {
        const auto& e = m_layout[i];
        if (e.top + e.height < clip.top())    continue;
        if (e.top             > clip.bottom()) break;

        const int x = std::max(0, (viewW - e.width) / 2);
        const QRect pageRect(x, e.top, e.width, e.height);

        // Sombra suave — múltiplas camadas simulam blur
        p.fillRect(pageRect.adjusted(-3, -1,  3, 3).translated(0, 8), QColor(0, 0, 0, 15));
        p.fillRect(pageRect.adjusted(-2,  0,  2, 2).translated(0, 6), QColor(0, 0, 0, 25));
        p.fillRect(pageRect.adjusted(-1,  0,  1, 1).translated(0, 4), QColor(0, 0, 0, 45));
        p.fillRect(pageRect.translated(0, 3),                          QColor(0, 0, 0, 60));

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

    // ── Highlights permanentes (desenhados ANTES da seleção) ──────────────
    // Os mergedPtRects são calculados uma vez em addHighlight() e reutilizados
    // aqui sem custo.  Para highlights carregados do disco (mergedPtRects vazio),
    // ensureMergedRects() preenche na primeira pintura.
    if (!m_highlights.isEmpty()) {
        for (auto& hl : m_highlights) {
            ensureMergedRects(hl);    // no-op após a primeira chamada
            for (const auto& span : hl.spans) {
                const QRect pr = pageRectInWidget(span.page);
                if (!pr.intersects(clip)) continue;

                QVector<QRectF> widgetRects;
                widgetRects.reserve(span.mergedPtRects.size());
                for (const QRectF& ptR : span.mergedPtRects) {
                    widgetRects.append(QRectF(
                        ptR.x()      * scale + pr.x(),
                        ptR.y()      * scale + pr.y(),
                        ptR.width()  * scale,
                        ptR.height() * scale));
                }
                paintHighlightRects(p, widgetRects, hl.color, HL_CORNER_RADIUS);
            }
        }
    }

    // ── Overlay de resultados de busca (temporários, cor azul-ciano) ─────
    if (m_searchHlPage >= 0 && !m_searchHlRects.isEmpty()) {
        const QRect pr = pageRectInWidget(m_searchHlPage);
        if (pr.intersects(clip)) {
            static const QColor kSearchColor(0x1A, 0xC4, 0xFF, 120); // azul-ciano
            QVector<QRectF> widgetRects;
            widgetRects.reserve(m_searchHlRects.size());
            for (const QRectF& r : m_searchHlRects) {
                widgetRects.append(QRectF(
                    r.x()      * scale + pr.x(),
                    r.y()      * scale + pr.y(),
                    r.width()  * scale,
                    r.height() * scale));
            }
            paintHighlightRects(p, widgetRects, kSearchColor, HL_CORNER_RADIUS);
        }
    }

    // ── Overlay de seleção de texto ───────────────────────────────────────
    // paintSelection() é chamado uma única vez — elimina o bloco duplicado
    // que existia separadamente dentro de m_selecting/TextFlowMode.
    paintSelection(p, clip);

    // ── Rubber-band retangular (RectMode durante o drag) ──────────────────
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
// keyPressEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::keyPressEvent(QKeyEvent* event)
{
    auto* sa = scrollArea();
    QScrollBar* vsb = sa ? sa->verticalScrollBar() : nullptr;

    const int lineStep = 48;
    const int pageStep = sa ? sa->viewport()->height() - lineStep : 600;

    switch (event->key()) {
    case Qt::Key_Down:      if (vsb) vsb->setValue(vsb->value() + lineStep); break;
    case Qt::Key_Up:        if (vsb) vsb->setValue(vsb->value() - lineStep); break;
    case Qt::Key_Space:
    case Qt::Key_PageDown:  if (vsb) vsb->setValue(vsb->value() + pageStep); break;
    case Qt::Key_Backspace:
    case Qt::Key_PageUp:    if (vsb) vsb->setValue(vsb->value() - pageStep); break;
    case Qt::Key_Left:      goToPage(m_currentPage - 1); break;
    case Qt::Key_Right:     goToPage(m_currentPage + 1); break;
    case Qt::Key_Home:      goToPage(0);               break;
    case Qt::Key_End:       goToPage(m_pageCount - 1); break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:     zoomIn();  break;
    case Qt::Key_Minus:     zoomOut(); break;
    case Qt::Key_0:
        if (event->modifiers() & Qt::ControlModifier) zoomReset();
        break;
    case Qt::Key_C:
        if (event->modifiers() & Qt::ControlModifier) copyToClipboard();
        break;
    case Qt::Key_Escape:    clearSelection(); break;
    case Qt::Key_Tab:
        if (event->modifiers() == Qt::NoModifier) toggleSelectionMode();
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
void PdfCanvasView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildLayout
// ─────────────────────────────────────────────────────────────────────────────
// Reconstrói o mapeamento página→rect usando m_pageSizes (pré-carregado em
// setDocument).  É chamada a cada frame da animação de zoom, portanto deve ser
// O(n) pura aritmética — sem chamadas ao Poppler.
void PdfCanvasView::rebuildLayout()
{
    m_layout.clear();
    m_layout.reserve(m_pageCount);

    const qreal scale = (BASE_DPI / 72.0) * m_zoom;
    int y = PAGE_GAP;
    for (int i = 0; i < m_pageCount; ++i) {
        const QSizeF ptSize = (i < m_pageSizes.size())
            ? m_pageSizes[i]
            : QSizeF(595.0, 842.0);

        const int w = qRound(ptSize.width()  * scale);
        const int h = qRound(ptSize.height() * scale);

        m_layout.append({y, w, h});
        y += h + PAGE_GAP;
    }

    m_totalHeight = y;
    setMinimumSize(0, m_totalHeight);
    updateGeometry();
}

// applyZoom — chamado pelos botões +/−/100% da barra de ferramentas.
// Ancora o zoom ao centro da viewport (comportamento neutro).
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::applyZoom(qreal newZoom)
{
    zoomAround(newZoom, viewportHeight() / 2, /*animate=*/true);
}

// ─────────────────────────────────────────────────────────────────────────────
// zoomAround (pública) — chamada pelo eventFilter do MainWindow para Ctrl+roda.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::zoomAround(qreal newZoom, int viewportAnchorY)
{
    zoomAround(newZoom, viewportAnchorY, /*animate=*/true);
}

// ─────────────────────────────────────────────────────────────────────────────
// applyZoomFactor — aplica fator multiplicativo acumulando sobre o alvo atual.
//
// Por que multiplicativo e não aditivo (ZOOM_STEP)?
//   Com zoom aditivo, 100%->115%->130% parece igual a 200%->215%->230%.
//   Mas visualmente 200%->215% é apenas 7.5%, enquanto 100%->115% é 15%.
//   O multiplicativo mantém a mesma proporção visual em qualquer nível.
//
// Acumulação sobre o alvo:
//   Se a animação está em andamento, aplicamos o fator sobre o zoom-alvo
//   (m_zoomAnim->endValue) e não sobre m_zoom (intermediário), evitando
//   a sensação de "rubber band" em Ctrl+roda rápido.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::applyZoomFactor(qreal factor, int viewportAnchorY)
{
    const qreal base = (m_zoomAnim->state() == QAbstractAnimation::Running)
        ? m_zoomAnim->endValue().toReal()
        : m_zoom;
    const qreal target = std::clamp(base * factor, ZOOM_MIN, ZOOM_MAX);
    zoomAround(target, viewportAnchorY, /*animate=*/true);
}

// ─────────────────────────────────────────────────────────────────────────────
// zoomAround (privada) — implementação central de todo zoom.
//
// Ancora de scroll:
//   O conteúdo em (scrollY + viewportAnchorY) permanece fixo apos o zoom.
//   anchorDocFrac = (scroll + anchorY) / totalHeight_old
//   scroll_new    = anchorDocFrac * totalHeight_new - anchorY
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::zoomAround(qreal newZoom, int viewportAnchorY, bool animate)
{
    newZoom = std::clamp(newZoom, ZOOM_MIN, ZOOM_MAX);

    const qreal targetZoom = (m_zoomAnim->state() == QAbstractAnimation::Running)
        ? m_zoomAnim->endValue().toReal()
        : m_zoom;
    if (qFuzzyCompare(newZoom, targetZoom)) return;

    int currentScroll = m_scrollY;
    if (auto* sa = scrollArea())
        currentScroll = sa->verticalScrollBar()->value();

    const int docY = currentScroll + viewportAnchorY;
    m_zoomAnchorFrac    = (m_totalHeight > 0)
        ? static_cast<qreal>(docY) / static_cast<qreal>(m_totalHeight)
        : 0.0;
    m_zoomAnchorScreenY = viewportAnchorY;

    if (!animate) {
        if (m_zoomAnim->state() == QAbstractAnimation::Running)
            m_zoomAnim->stop();
        m_zoom = newZoom;
        rebuildLayout();
        if (auto* sa = scrollArea()) {
            const int t = qRound(m_zoomAnchorFrac * m_totalHeight) - m_zoomAnchorScreenY;
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
// syncCurrentPage
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::syncCurrentPage(int scrollY)
{
    if (m_layout.isEmpty()) return;

    const int vpH      = viewportHeight();
    const int vpCenter = scrollY + vpH / 2;

    int best     = 0;
    int bestDist = INT_MAX;

    for (int i = 0; i < m_layout.size(); ++i) {
        const int pageCenter = m_layout[i].top + m_layout[i].height / 2;
        const int dist       = std::abs(pageCenter - vpCenter);
        if (dist < bestDist) { bestDist = dist; best = i; }
    }

    if (best != m_currentPage) {
        m_currentPage = best;
        emit currentPageChanged(m_currentPage);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// pageRectInWidget
// ─────────────────────────────────────────────────────────────────────────────
QRect PdfCanvasView::pageRectInWidget(int i) const
{
    if (i < 0 || i >= m_layout.size()) return {};
    const auto& e = m_layout[i];
    const int x = std::max(0, (width() - e.width) / 2);
    return QRect(x, e.top, e.width, e.height);
}

// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent — inicia o drag de seleção
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_toolMode == ToolMode::Pan) {
            m_panning   = true;
            m_panOrigin = event->globalPosition().toPoint();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        m_selThrottle->stop();
        m_selPending   = false;
        m_selecting    = true;
        m_hasSelection = false;
        m_selOrigin    = event->pos();
        m_selCurrent   = event->pos();
        m_pageSelections.clear();
        m_selectedText.clear();
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseMoveEvent — atualiza o drag
//
// TextFlowMode:
//   Antes: chamava computeTextFlowSelection() direto com um limiar ingênuo de
//   3 px (manhattanLength).  Problema: a cada pixel arrastado, pg->textList()
//   era chamado — centenas de ms bloqueados no event loop em PDFs densos.
//
//   Agora: registra a posição atual e agenda o throttle timer (16 ms).  O
//   timer dispara computeTextFlowSelection() que usa o word cache (getPageWords),
//   e depois faz update(dirtyRect) em vez de update() full-widget.
//
// RectMode:
//   Não precisa do word cache nem do throttle — apenas move o rubber-band
//   e chama update() na union do rect antigo + novo (dirty rect mínimo).
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning && (event->buttons() & Qt::LeftButton)) {
        const QPoint cur   = event->globalPosition().toPoint();
        const QPoint delta = cur - m_panOrigin;
        m_panOrigin = cur;
        if (auto* sa = scrollArea()) {
            sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - delta.x());
            sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - delta.y());
        }
        event->accept();
        return;
    }

    if (m_selecting && (event->buttons() & Qt::LeftButton)) {
        m_selCurrent = event->pos();

        if (m_selMode == SelectionMode::TextFlowMode) {
            // Agenda processamento no próximo tick do timer (16 ms)
            m_selPending = true;
            if (!m_selThrottle->isActive())
                m_selThrottle->start();
        } else {
            // RectMode: atualiza só a área coberta pelo rubber-band
            const QRect oldRubber = QRect(m_selOrigin, m_selCurrent).normalized();
            update(oldRubber.adjusted(-2, -2, 2, 2));
        }

        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseReleaseEvent — finaliza a seleção
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_selecting) {
        // Garante que o timer pendente é processado antes de finalizar
        m_selThrottle->stop();
        m_selPending   = false;
        m_selecting    = false;
        m_selCurrent   = event->pos();

        if (m_selMode == SelectionMode::RectMode)
            computeSelection();
        else
            computeTextFlowSelection();

        // ── Modo Anotar: converte seleção em highlight permanente ─────────
        if (m_toolMode == ToolMode::Annotate && m_hasSelection
                && !m_selectedText.isEmpty()) {
            HighlightEntry h;
            h.id   = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
            h.text = m_selectedText;
            for (const auto& ps : std::as_const(m_pageSelections)) {
                HighlightPageSpan span;
                span.page    = ps.page;
                span.ptRects = ps.ptRects;
                h.spans.append(span);
            }
            if (!h.spans.isEmpty())
                emit highlightRequested(h);

            m_hasSelection = false;
            m_pageSelections.clear();
            m_selectedText.clear();
        }

        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// highlightIdAtPoint
// ─────────────────────────────────────────────────────────────────────────────
QString PdfCanvasView::highlightIdAtPoint(const QPoint& widgetPos) const
{
    const qreal scale = (BASE_DPI / 72.0) * m_zoom;
    for (const auto& hl : std::as_const(m_highlights)) {
        for (const auto& span : hl.spans) {
            const QRect pr = pageRectInWidget(span.page);
            // Usa mergedPtRects se disponível (mais preciso para hit-testing)
            const QVector<QRectF>& rects = span.mergedPtRects.isEmpty()
                ? span.ptRects : span.mergedPtRects;
            for (const QRectF& ptR : rects) {
                const QRectF wr(
                    ptR.x()      * scale + pr.x(),
                    ptR.y()      * scale + pr.y(),
                    ptR.width()  * scale,
                    ptR.height() * scale);
                if (wr.contains(widgetPos))
                    return hl.id;
            }
        }
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// contextMenuEvent
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    const QString hlId = highlightIdAtPoint(event->pos());
    if (!hlId.isEmpty()) {
        QAction* actDel = menu.addAction(tr("Excluir highlight"));
        connect(actDel, &QAction::triggered, this, [this, hlId]() {
            emit removeHighlightRequested(hlId);
        });
        menu.addSeparator();
    }

    if (m_hasSelection) {
        QAction* actCopy = menu.addAction(
            QIcon::fromTheme(QStringLiteral("edit-copy")),
            tr("Copiar texto selecionado\tCtrl+C"));
        connect(actCopy, &QAction::triggered, this, &PdfCanvasView::copyToClipboard);
        menu.addSeparator();
    }

    const bool isFlow = (m_selMode == SelectionMode::TextFlowMode);
    QAction* actToggle = menu.addAction(
        isFlow ? tr("Mudar para: Seleção retangular")
               : tr("Mudar para: Seleção por fluxo de texto"));
    connect(actToggle, &QAction::triggered, this, &PdfCanvasView::toggleSelectionMode);

    menu.exec(event->globalPos());
    event->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// computeSelection — RectMode
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::computeSelection()
{
    m_pageSelections.clear();
    m_selectedText.clear();
    m_hasSelection = false;

    if (!m_doc || m_layout.isEmpty()) return;

    const QRect sel = QRect(m_selOrigin, m_selCurrent).normalized();
    if (sel.isEmpty()) return;

    const qreal scale = (BASE_DPI / 72.0) * m_zoom;

    for (int i = 0; i < m_pageCount; ++i) {
        const QRect pr    = pageRectInWidget(i);
        const QRect inter = pr.intersected(sel);
        if (inter.isEmpty()) continue;

        const QRect local(inter.x() - pr.x(), inter.y() - pr.y(),
                          inter.width(), inter.height());
        const QRectF ptRect(
            local.x()      / scale, local.y()      / scale,
            local.width()  / scale, local.height() / scale);

        auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
        if (!pg) continue;

        const QString pageText = pg->text(ptRect);
        if (pageText.trimmed().isEmpty()) continue;

        m_pageSelections.append({i, {ptRect}});
        if (!m_selectedText.isEmpty()) m_selectedText += QLatin1Char('\n');
        m_selectedText += pageText;
    }

    m_hasSelection = !m_selectedText.isEmpty();
    if (m_hasSelection)
        emit textSelected(m_selectedText);
}

// ─────────────────────────────────────────────────────────────────────────────
// widgetPointToPagePt
// ─────────────────────────────────────────────────────────────────────────────
bool PdfCanvasView::widgetPointToPagePt(const QPoint& wp,
                                        int* outPage,
                                        qreal* outPtX, qreal* outPtY) const
{
    const qreal scale = (BASE_DPI / 72.0) * m_zoom;
    for (int i = 0; i < m_pageCount; ++i) {
        const QRect pr = pageRectInWidget(i);
        if (pr.contains(wp)) {
            *outPage = i;
            *outPtX  = (wp.x() - pr.x()) / scale;
            *outPtY  = (wp.y() - pr.y()) / scale;
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// computeTextFlowSelection — TextFlowMode
//
// Principais melhorias em relação à versão original:
//
//   1. Word cache (getPageWords): Poppler::Page::textList() é chamado no
//      máximo UMA vez por página por sessão de leitura.  Em PDFs de 400 páginas
//      com texto denso, isso reduz o tempo de processamento de ~8 ms/frame para
//      < 0,5 ms/frame após o primeiro acesso.
//
//   2. Sem allocações de Page por frame: o loop usa apenas o cache de WordInfo
//      (QVector<QRectF> de charBoxes), evitando alocações de heap durante o drag.
//
//   3. Chamado pelo throttle timer (16 ms), não diretamente em mouseMoveEvent.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::computeTextFlowSelection()
{
    m_pageSelections.clear();
    m_selectedText.clear();
    m_hasSelection = false;

    if (!m_doc || m_layout.isEmpty()) return;

    QPoint topWidget = m_selOrigin;
    QPoint botWidget = m_selCurrent;
    if (topWidget.y() > botWidget.y()) std::swap(topWidget, botWidget);

    int   startPage = -1, endPage = -1;
    qreal startPtX = 0, startPtY = 0, endPtX = 0, endPtY = 0;

    widgetPointToPagePt(topWidget, &startPage, &startPtX, &startPtY);
    widgetPointToPagePt(botWidget, &endPage,   &endPtX,   &endPtY);

    if (startPage < 0) {
        for (int i = 0; i < m_pageCount; ++i) {
            if (pageRectInWidget(i).top() >= topWidget.y())
                { startPage = i; startPtX = 0; startPtY = 0; break; }
        }
        if (startPage < 0) startPage = 0;
    }
    if (endPage < 0) {
        for (int i = m_pageCount - 1; i >= 0; --i) {
            const QRect pr = pageRectInWidget(i);
            if (pr.bottom() <= botWidget.y()) {
                // Usa cache se disponível, senão cria página temporariamente
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
        if (endPage < 0) endPage = m_pageCount - 1;
    }
    if (startPage > endPage) std::swap(startPage, endPage);

    for (int i = startPage; i <= endPage; ++i) {
        const QVector<WordInfo> words = getPageWords(i);
        if (words.isEmpty()) continue;

        const bool isFirst  = (i == startPage);
        const bool isLast   = (i == endPage);
        const bool isSingle = isFirst && isLast;

        QVector<QRectF> rects;
        QString         pageText;
        bool            prevHadSpace = false;

        for (const WordInfo& wi : words) {
            const qreal cy    = wi.bbox.center().y();
            const qreal hh    = wi.bbox.height() * 0.6;
            const int   nChars = wi.text.length();

            if (nChars == 0) continue;

            bool wordAddedAnyChar = false;

            for (int c = 0; c < nChars; ++c) {
                const QRectF& charBox = wi.charBoxes.value(c);
                if (charBox.isEmpty()) continue;

                const qreal ccx    = charBox.center().x();
                bool        charIn = false;

                if (isSingle) {
                    const bool onStart  = qAbs(cy - startPtY) <= hh;
                    const bool onEnd    = qAbs(cy - endPtY)   <= hh;
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
            if (!m_selectedText.isEmpty()) m_selectedText += QLatin1Char('\n');
            m_selectedText += pageText;
        }
    }

    m_hasSelection = !m_selectedText.isEmpty();
    if (!m_selecting && m_hasSelection)
        emit textSelected(m_selectedText);
}

// ─────────────────────────────────────────────────────────────────────────────
// clearSelection
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::clearSelection()
{
    m_selThrottle->stop();
    m_selPending   = false;
    m_selecting    = false;
    m_hasSelection = false;
    m_pageSelections.clear();
    m_selectedText.clear();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// setSelectionMode / toggleSelectionMode
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setSelectionMode(SelectionMode m)
{
    if (m == m_selMode) return;
    m_selMode = m;
    clearSelection();
    setCursor(m == SelectionMode::TextFlowMode ? Qt::IBeamCursor : Qt::CrossCursor);
    emit selectionModeChanged(m_selMode);
}

void PdfCanvasView::toggleSelectionMode()
{
    setSelectionMode(m_selMode == SelectionMode::TextFlowMode
                     ? SelectionMode::RectMode
                     : SelectionMode::TextFlowMode);
}

// ─────────────────────────────────────────────────────────────────────────────
// setToolMode
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setToolMode(ToolMode mode)
{
    if (m_toolMode == mode) return;
    m_toolMode = mode;
    m_panning  = false;

    if (mode == ToolMode::Pan)
        setCursor(Qt::OpenHandCursor);
    else if (mode == ToolMode::Annotate)
        setCursor(Qt::IBeamCursor);
    else
        setCursor(m_selMode == SelectionMode::TextFlowMode
                  ? Qt::IBeamCursor : Qt::CrossCursor);

    emit toolModeChanged(mode);
}

// ─────────────────────────────────────────────────────────────────────────────
// normalizeForClipboard
// ─────────────────────────────────────────────────────────────────────────────
// Limpa o texto extraído do PDF antes de ir para o clipboard:
//
//   1. Remove soft-hyphens (U+00AD) — artefatos invisíveis que aparecem como
//      caracteres estranhos em alguns editores de texto.
//
//   2. Desfaz hifenização de linha/página — quando uma palavra é partida no
//      fim da linha com hífen ("pala-\nbra"), une as partes ("palavra").
//      Aplica tanto ao hífen normal (U+002D) quanto ao traço não-quebrável
//      (U+2011).
//
//   3. Substitui quebras de linha simples por espaço — `\n` isolado é quebra
//      de linha automática do PDF, não separador de parágrafo real.
//      Dois `\n` seguidos indicam parágrafo real e são preservados.
//
//   4. Normaliza espaços múltiplos — após as substituições podem sobrar
//      sequências de espaços; reduz para um único espaço.
//
//   5. Remove espaços residuais antes de quebra de parágrafo ("texto \n\n").
//
// ─────────────────────────────────────────────────────────────────────────────
static QString normalizeForClipboard(const QString& raw)
{
    QString s = raw;

    // 1. Soft-hyphens invisíveis (U+00AD)
    s.remove(QChar(0x00AD));

    // 2. Desfaz hifenização no fim de linha/página.
    //    Cobre hífen normal (U+002D) e hífen não-quebrável (U+2011).
    static const QRegularExpression reHyphen(
        QStringLiteral("[\\x{002D}\\x{2011}]\\n"),
        QRegularExpression::UseUnicodePropertiesOption);
    s.replace(reHyphen, QString());

    // 3. \n simples → espaço  |  \n\n (parágrafo real) → preservado.
    //    Estratégia: marca os parágrafos reais com sentinela, colapsa \n
    //    restantes para espaço, restaura a sentinela.
    const QString kParaMark = QStringLiteral("\x00\x01");
    s.replace(QLatin1String("\n\n"), kParaMark);
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(kParaMark, QLatin1String("\n\n"));

    // 4. Colapsa múltiplos espaços em um só.
    static const QRegularExpression reSpaces(QStringLiteral("  +"));
    s.replace(reSpaces, QStringLiteral(" "));

    // 5. Espaço residual antes de quebra de parágrafo.
    static const QRegularExpression reTrail(QStringLiteral(" \\n\\n"));
    s.replace(reTrail, QStringLiteral("\n\n"));

    return s.trimmed();
}

// copyToClipboard
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::copyToClipboard()
{
    if (!m_selectedText.isEmpty())
        QApplication::clipboard()->setText(normalizeForClipboard(m_selectedText));
}

// ─────────────────────────────────────────────────────────────────────────────
// addHighlight
//
// Pré-calcula mergedPtRects para cada span imediatamente após receber o
// highlight.  Isso move o custo de mergeRectsByLine() para cá (ocorre uma única
// vez, ao criar o highlight) e elimina o cálculo do paintEvent, que rodava
// a cada frame em todo highlight visível.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::addHighlight(const HighlightEntry& h)
{
    HighlightEntry entry = h;
    ensureMergedRects(entry);

    for (auto& existing : m_highlights) {
        if (existing.id == entry.id) {
            existing = entry;
            update();
            return;
        }
    }
    m_highlights.append(entry);
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// removeHighlight / clearHighlights
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::removeHighlight(const QString& id)
{
    const int before = m_highlights.size();
    m_highlights.erase(
        std::remove_if(m_highlights.begin(), m_highlights.end(),
                       [&id](const HighlightEntry& h){ return h.id == id; }),
        m_highlights.end());
    if (m_highlights.size() != before) update();
}

void PdfCanvasView::clearHighlights()
{
    if (m_highlights.isEmpty()) return;
    m_highlights.clear();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// setSearchHighlights / clearSearchHighlights
//
// Exibe um overlay azul-ciano temporário sobre os resultados da busca ativa.
// Chamado por MainWindow::runSearch ao clicar num item da lista de resultados.
// ptRects devem estar em coordenadas de pontos PDF (mesma unidade do Poppler),
// pois a conversão para widget-coords é feita em paintEvent usando BASE_DPI*zoom.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setSearchHighlights(int page, const QList<QRectF>& ptRects)
{
    m_searchHlPage  = page;
    m_searchHlRects = ptRects;
    update();
}

void PdfCanvasView::clearSearchHighlights()
{
    if (m_searchHlPage < 0 && m_searchHlRects.isEmpty()) return;
    m_searchHlPage  = -1;
    m_searchHlRects.clear();
    update();
}
