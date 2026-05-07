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
#include <climits>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
PdfCanvasView::PdfCanvasView(QWidget* parent)
    : QWidget(parent)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x2b, 0x2b, 0x2b));
    setAutoFillBackground(true);
    setPalette(pal);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);   // indica ao usuário que o texto é selecionável
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

    rebuildLayout();
    update();
    emit currentPageChanged(0);
    emit zoomChanged(1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// sizeHint — altura total do documento; QScrollArea usa para definir o range.
// A largura é ignorada pelo QScrollArea (widgetResizable preenche o viewport).
// ─────────────────────────────────────────────────────────────────────────────
QSize PdfCanvasView::sizeHint() const
{
    return QSize(800, m_totalHeight > 0 ? m_totalHeight : 600);
}

// ─────────────────────────────────────────────────────────────────────────────
// goToPage — rola instantaneamente até a página (chamado pelos botões ←/→)
//
// BUG CORRIGIDO: parentWidget() retorna o *viewport* do QScrollArea, não o
// QScrollArea em si.  O helper scrollArea() sobe dois níveis corretamente.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::goToPage(int page)
{
    if (m_pageCount == 0 || m_layout.isEmpty()) return;
    page = std::clamp(page, 0, m_pageCount - 1);

    // ← era: qobject_cast<QScrollArea*>(parentWidget())  →  sempre nullptr
    if (auto* sa = scrollArea())
        sa->verticalScrollBar()->setValue(m_layout[page].top - PAGE_GAP);
}

// ─────────────────────────────────────────────────────────────────────────────
// onScrollChanged — recebe o offset do scrollbar para rastrear a página atual
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::onScrollChanged(int scrollY)
{
    m_scrollY = scrollY;
    syncCurrentPage(scrollY);
}

// ─────────────────────────────────────────────────────────────────────────────
// requestRepaintPage — chamado pelo PageCache::pageReady
//
// BUG CORRIGIDO: a verificação de visibilidade usava height() (= m_totalHeight,
// toda a altura do widget) em vez da altura real do viewport visível.  Com isso
// quase todas as páginas eram agendadas para repaint desnecessariamente.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::requestRepaintPage(int page)
{
    if (page < 0 || page >= m_layout.size()) return;
    const auto& e = m_layout[page];

    // ← era: m_scrollY + height()  →  height() é a altura total do widget,
    //   não a do viewport.  viewportHeight() retorna o tamanho correto.
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
// paintEvent — renderiza apenas as páginas visíveis na rect suja
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Placeholder quando nenhum documento está aberto
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

    const QRect clip  = event->rect();  // área suja em coordenadas do widget
    const int   viewW = width();

    for (int i = 0; i < m_pageCount; ++i) {
        const auto& e = m_layout[i];

        // ── Viewport culling ──────────────────────────────────────────────
        if (e.top + e.height < clip.top())    continue;
        if (e.top             > clip.bottom()) break;

        // ── Centraliza horizontalmente ────────────────────────────────────
        const int x = std::max(0, (viewW - e.width) / 2);
        const QRect pageRect(x, e.top, e.width, e.height);

        // ── Sombra ────────────────────────────────────────────────────────
        p.fillRect(pageRect.translated(4, 4), QColor(0, 0, 0, 80));

        // ── Conteúdo da página ────────────────────────────────────────────
        const auto pixOpt = m_cache->get(i);
        if (pixOpt.has_value()) {
            p.drawPixmap(pageRect, pixOpt.value());
        } else {
            // Página ainda renderizando: fundo branco + texto de loading
            p.fillRect(pageRect, Qt::white);
            p.setPen(QColor(0x99, 0x99, 0x99));
            QFont f = p.font();
            f.setPointSize(11);
            p.setFont(f);
            p.drawText(pageRect, Qt::AlignCenter,
                       QStringLiteral("Carregando página %1…").arg(i + 1));
        }
    }

    // ── Overlay de seleção de texto ───────────────────────────────────────
    // Desenhado após as páginas para ficar por cima do conteúdo.

    // 1) Highlight azul sobre as bounding boxes confirmadas (mouse solto)
    if (m_hasSelection) {
        const qreal scale = (BASE_DPI / 72.0) * m_zoom;
        p.save();
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        const QColor hlColor(0x33, 0x99, 0xFF, 90);
        for (const auto& ps : std::as_const(m_pageSelections)) {
            const QRect pr = pageRectInWidget(ps.page);
            if (!pr.intersects(clip)) continue;
            for (const QRectF& ptR : ps.ptRects) {
                const QRectF pixRect(
                    ptR.x()      * scale + pr.x(),
                    ptR.y()      * scale + pr.y(),
                    ptR.width()  * scale,
                    ptR.height() * scale);
                p.fillRect(pixRect, hlColor);
            }
        }
        p.restore();
    }

    // 2) Feedback visual durante o drag
    if (m_selecting) {
        if (m_selMode == SelectionMode::RectMode) {
            // Rect de borracha tracejado
            const QRect selRect = QRect(m_selOrigin, m_selCurrent).normalized();
            if (!selRect.isEmpty()) {
                p.save();
                p.setPen(QPen(QColor(0x33, 0x99, 0xFF, 220), 1, Qt::DashLine));
                p.setBrush(QColor(0x33, 0x99, 0xFF, 30));
                p.drawRect(selRect);
                p.restore();
            }
        } else {
            // TextFlowMode: highlight em tempo real das palavras já encontradas
            if (m_hasSelection) {
                const qreal scale = (BASE_DPI / 72.0) * m_zoom;
                p.save();
                p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                const QColor hlColor(0x33, 0x99, 0xFF, 90);
                for (const auto& ps : std::as_const(m_pageSelections)) {
                    const QRect pr = pageRectInWidget(ps.page);
                    if (!pr.intersects(clip)) continue;
                    for (const QRectF& ptR : ps.ptRects) {
                        const QRectF pixRect(
                            ptR.x()      * scale + pr.x(),
                            ptR.y()      * scale + pr.y(),
                            ptR.width()  * scale,
                            ptR.height() * scale);
                        p.fillRect(pixRect, hlColor);
                    }
                }
                p.restore();
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// keyPressEvent — navegação por teclado
//
// BUG CORRIGIDO: mesma raiz do goToPage — parentWidget() era o viewport.
// Agora usa o helper scrollArea().
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::keyPressEvent(QKeyEvent* event)
{
    // ← era: qobject_cast<QScrollArea*>(parentWidget())  →  sempre nullptr,
    //   portanto vsb ficava nullptr e nenhuma tecla de rolagem funcionava.
    auto* sa = scrollArea();
    QScrollBar* vsb = sa ? sa->verticalScrollBar() : nullptr;

    const int lineStep = 48;
    const int pageStep = sa ? sa->viewport()->height() - lineStep : 600;

    switch (event->key()) {

    case Qt::Key_Down:
        if (vsb) vsb->setValue(vsb->value() + lineStep);
        break;
    case Qt::Key_Up:
        if (vsb) vsb->setValue(vsb->value() - lineStep);
        break;
    case Qt::Key_Space:
    case Qt::Key_PageDown:
        if (vsb) vsb->setValue(vsb->value() + pageStep);
        break;
    case Qt::Key_Backspace:
    case Qt::Key_PageUp:
        if (vsb) vsb->setValue(vsb->value() - pageStep);
        break;

    // Salta para página anterior/próxima (snap ao topo da página)
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
    case Qt::Key_C:
        if (event->modifiers() & Qt::ControlModifier)
            copyToClipboard();
        break;
    case Qt::Key_Escape:
        clearSelection();
        break;
    // Tab (sem modificadores) alterna entre os dois modos de seleção
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
void PdfCanvasView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildLayout — recalcula posições e tamanhos de todas as páginas.
// Chamado na inicialização e ao mudar o zoom.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::rebuildLayout()
{
    m_layout.clear();
    m_layout.reserve(m_pageCount);

    int y = PAGE_GAP;

    for (int i = 0; i < m_pageCount; ++i) {
        // Tamanho da página em pontos (72 DPI) → pixels no BASE_DPI × zoom
        QSizeF ptSize;
        if (m_doc) {
            if (auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i)))
                ptSize = pg->pageSizeF();
        }
        if (ptSize.isEmpty())
            ptSize = QSizeF(595.0, 842.0);  // fallback A4

        const qreal scale = (BASE_DPI / 72.0) * m_zoom;
        const int w = qRound(ptSize.width()  * scale);
        const int h = qRound(ptSize.height() * scale);

        m_layout.append({y, w, h});
        y += h + PAGE_GAP;
    }

    m_totalHeight = y;

    // ── Atualiza o tamanho mínimo do widget ───────────────────────────────
    // CORREÇÃO CRÍTICA: com setWidgetResizable(true), o QScrollArea dimensiona
    // o widget pelo minimumSize(), NÃO pelo sizeHint().  Sem esta chamada,
    // o widget encolhe até o tamanho do viewport → range do scrollbar = 0 →
    // nenhum scroll possível.
    //
    // setMinimumHeight(m_totalHeight) força o constraint de altura mínima.
    // QScrollArea responde redimensionando o widget para m_totalHeight e
    // ativando a barra vertical automaticamente.
    //
    // A largura mínima 0 preserva o comportamento de "preencher viewport"
    // que o widgetResizable=true fornece na dimensão horizontal.
    setMinimumSize(0, m_totalHeight);

    // updateGeometry() notifica o layout pai que as preferências mudaram.
    // Necessário além do setMinimumSize() para que o QScrollArea
    // recalcule imediatamente o range sem esperar o próximo repaint.
    updateGeometry();
}

// ─────────────────────────────────────────────────────────────────────────────
// applyZoom — muda o zoom preservando a posição relativa da página atual
//
// BUG CORRIGIDO: parentWidget() retornava o viewport.  Usa scrollArea().
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::applyZoom(qreal newZoom)
{
    newZoom = std::clamp(newZoom, ZOOM_MIN, ZOOM_MAX);
    if (qFuzzyCompare(newZoom, m_zoom)) return;

    // Ancora no topo da página atual para não perder o contexto de leitura
    const qreal anchorFrac = (m_totalHeight > 0 && !m_layout.isEmpty())
        ? static_cast<qreal>(m_layout[m_currentPage].top) / m_totalHeight
        : 0.0;

    m_zoom = newZoom;
    rebuildLayout();

    // ← era: qobject_cast<QScrollArea*>(parentWidget())  →  sempre nullptr
    if (auto* sa = scrollArea())
        sa->verticalScrollBar()->setValue(qRound(anchorFrac * m_totalHeight));

    update();
    emit zoomChanged(m_zoom);
}

// ─────────────────────────────────────────────────────────────────────────────
// syncCurrentPage — a página atual é aquela cujo centro está mais próximo
//                   do centro do viewport visível.
//
// viewportHeight() retorna a altura do viewport (pai direto = viewport),
// que é o valor correto aqui — não a altura total do widget.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::syncCurrentPage(int scrollY)
{
    if (m_layout.isEmpty()) return;

    const int vpH      = viewportHeight();   // ← altura real do viewport
    const int vpCenter = scrollY + vpH / 2;

    int best     = 0;
    int bestDist = INT_MAX;

    for (int i = 0; i < m_layout.size(); ++i) {
        const int pageCenter = m_layout[i].top + m_layout[i].height / 2;
        const int dist       = std::abs(pageCenter - vpCenter);
        if (dist < bestDist) {
            bestDist = dist;
            best     = i;
        }
    }

    if (best != m_currentPage) {
        m_currentPage = best;
        emit currentPageChanged(m_currentPage);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// pageRectInWidget — QRect da página i em coordenadas do widget
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
//   • RectMode:     só move o rubber-band (sem tocar em Poppler)
//   • TextFlowMode: recalcula as palavras selecionadas em tempo real
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning && (event->buttons() & Qt::LeftButton)) {
        const QPoint cur   = event->globalPosition().toPoint();
        const QPoint delta = cur - m_panOrigin;
        m_panOrigin = cur;

        if (auto* sa = scrollArea()) {
            auto* hb = sa->horizontalScrollBar();
            auto* vb = sa->verticalScrollBar();
            hb->setValue(hb->value() - delta.x());
            vb->setValue(vb->value() - delta.y());
        }
        event->accept();
        return;
    }
    if (m_selecting && (event->buttons() & Qt::LeftButton)) {
        const QPoint prev = m_selCurrent;
        m_selCurrent = event->pos();

        if (m_selMode == SelectionMode::TextFlowMode) {
            // Evita recalcular se o mouse mal se moveu (limiar 3 px)
            if ((m_selCurrent - prev).manhattanLength() >= 3)
                computeTextFlowSelection();
        }

        update();
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
        m_selecting  = false;
        m_selCurrent = event->pos();

        if (m_selMode == SelectionMode::RectMode)
            computeSelection();
        else
            computeTextFlowSelection();

        update();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// contextMenuEvent — "Copiar" quando há seleção; "Modo de seleção" sempre
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    if (m_hasSelection) {
        QAction* actCopy = menu.addAction(
            QIcon::fromTheme(QStringLiteral("edit-copy")),
            tr("Copiar texto selecionado\tCtrl+C"));
        connect(actCopy, &QAction::triggered, this, &PdfCanvasView::copyToClipboard);
        menu.addSeparator();
    }

    // Toggle de modo — mostra qual está ativo
    const bool isFlow = (m_selMode == SelectionMode::TextFlowMode);
    QAction* actToggle = menu.addAction(
        isFlow ? tr("Mudar para: Seleção retangular")
               : tr("Mudar para: Seleção por fluxo de texto"));
    connect(actToggle, &QAction::triggered, this, &PdfCanvasView::toggleSelectionMode);

    menu.exec(event->globalPos());
    event->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// computeSelection — mapeia o rect de seleção para coordenadas Poppler e
//                    extrai o texto de cada página intersectada
//
// Fluxo de coordenadas
// ────────────────────
// Widget px  →  página-local px  →  pontos Poppler (÷ scale)
//
// scale = (BASE_DPI / 72.0) × m_zoom
//
// Poppler::Page::text(QRectF) espera um rect em pontos com origem no
// canto superior-esquerdo da página (mesma orientação do renderizado).
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

        // Converte para coords locais dentro da página (em pixels)
        const QRect local(inter.x() - pr.x(),
                          inter.y() - pr.y(),
                          inter.width(),
                          inter.height());

        // Converte pixels → pontos Poppler
        const QRectF ptRect(
            local.x()      / scale,
            local.y()      / scale,
            local.width()  / scale,
            local.height() / scale);

        // Poppler::Page é criado por demanda; thread-safe no main thread.
        auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
        if (!pg) continue;

        const QString pageText = pg->text(ptRect);
        if (pageText.trimmed().isEmpty()) continue;

        m_pageSelections.append({i, {ptRect}});   // ← QVector com 1 rect

        if (!m_selectedText.isEmpty())
            m_selectedText += QLatin1Char('\n');
        m_selectedText += pageText;
    }

    m_hasSelection = !m_selectedText.isEmpty();
    if (m_hasSelection)
        emit textSelected(m_selectedText);
}

// ─────────────────────────────────────────────────────────────────────────────
// widgetPointToPagePt — converte coordenadas do widget para espaço Poppler
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
// computeTextFlowSelection — seleciona palavra por palavra seguindo o fluxo
//                            do texto (como num browser ou leitor de PDF).
//
// Algoritmo por página:
//   • Página inicial : a partir da linha do cursor de início
//                      (mesma linha: só palavras à direita do cursor)
//   • Páginas do meio: todas as palavras
//   • Página final   : até a linha do cursor de fim
//                      (mesma linha: só palavras à esquerda do cursor)
//
// "mesma linha" = |cy − cursorY| ≤ 60 % da altura da palavra.
// Poppler::Page::textList() devolve os TextBox em ordem de leitura.
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::computeTextFlowSelection()
{
    m_pageSelections.clear();
    m_selectedText.clear();
    m_hasSelection = false;

    if (!m_doc || m_layout.isEmpty()) return;

    // Normaliza: topPt sempre acima de botPt
    QPoint topWidget = m_selOrigin;
    QPoint botWidget = m_selCurrent;
    if (topWidget.y() > botWidget.y()) std::swap(topWidget, botWidget);

    // Resolve páginas e coordenadas de início/fim
    int   startPage = -1, endPage = -1;
    qreal startPtX = 0,   startPtY = 0;
    qreal endPtX   = 0,   endPtY   = 0;

    widgetPointToPagePt(topWidget, &startPage, &startPtX, &startPtY);
    widgetPointToPagePt(botWidget, &endPage,   &endPtX,   &endPtY);

    // Se o cursor caiu no gap entre páginas, snapa para a mais próxima
    if (startPage < 0) {
        for (int i = 0; i < m_pageCount; ++i) {
            if (pageRectInWidget(i).top() >= topWidget.y())
                { startPage = i; startPtX = 0; startPtY = 0; break; }
        }
        if (startPage < 0) startPage = 0;
    }
    if (endPage < 0) {
        for (int i = m_pageCount - 1; i >= 0; --i) {
            if (pageRectInWidget(i).bottom() <= botWidget.y()) {
                auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
                endPage = i;
                endPtX  = pg ? pg->pageSizeF().width()  : 1000.0;
                endPtY  = pg ? pg->pageSizeF().height() : 1000.0;
                break;
            }
        }
        if (endPage < 0) endPage = m_pageCount - 1;
    }
    if (startPage > endPage) std::swap(startPage, endPage);

    // Processa cada página no intervalo
    for (int i = startPage; i <= endPage; ++i) {
        auto pg = std::unique_ptr<Poppler::Page>(m_doc->page(i));
        if (!pg) continue;

        // Poppler-Qt6 >= 21.01: textList() retorna
        // std::vector<std::unique_ptr<Poppler::TextBox>>
        const auto words = pg->textList();
        if (words.empty()) continue;

        const bool isFirst  = (i == startPage);
        const bool isLast   = (i == endPage);
        const bool isSingle = isFirst && isLast;

        QVector<QRectF> rects;
        QString         pageText;
        bool            prevHadSpace = false;

        for (const auto& word : words) {
            const QRectF box = word->boundingBox();
            const qreal  cy  = box.center().y();
            const qreal  cx  = box.center().x();
            const qreal  hh  = box.height() * 0.6;

            bool inSel = false;

            if (isSingle) {
                const bool onStart  = std::abs(cy - startPtY) <= hh;
                const bool onEnd    = std::abs(cy - endPtY)   <= hh;
                const bool betweenY = (cy > startPtY + hh) && (cy < endPtY - hh);

                if (betweenY) {
                    inSel = true;
                } else if (onStart && onEnd) {
                    const qreal xMin = std::min(startPtX, endPtX);
                    const qreal xMax = std::max(startPtX, endPtX);
                    inSel = (cx >= xMin && cx <= xMax);
                } else if (onStart) {
                    inSel = (cx >= startPtX);
                } else if (onEnd) {
                    inSel = (cx <= endPtX);
                }
            } else if (isFirst) {
                const bool onStartLine = std::abs(cy - startPtY) <= hh;
                inSel = onStartLine ? (cx >= startPtX) : (cy > startPtY - hh);
            } else if (isLast) {
                const bool onEndLine = std::abs(cy - endPtY) <= hh;
                inSel = onEndLine ? (cx <= endPtX) : (cy < endPtY + hh);
            } else {
                inSel = true;
            }

            if (inSel) {
                rects.append(box);
                if (!pageText.isEmpty() && prevHadSpace)
                    pageText += QLatin1Char(' ');
                pageText += word->text();
                prevHadSpace = word->hasSpaceAfter();
            }
        }

        // unique_ptr — sem qDeleteAll necessário

        if (!rects.isEmpty()) {
            m_pageSelections.append({i, rects});
            if (!m_selectedText.isEmpty())
                m_selectedText += QLatin1Char('\n');
            m_selectedText += pageText;
        }
    }

    m_hasSelection = !m_selectedText.isEmpty();
    // Só emite no mouseRelease, não durante o drag em tempo real
    if (!m_selecting && m_hasSelection)
        emit textSelected(m_selectedText);
}

// ─────────────────────────────────────────────────────────────────────────────
// clearSelection
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::clearSelection()
{
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
// setToolMode — alterna entre Select e Pan (mãozinha)
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::setToolMode(ToolMode mode)
{
    if (m_toolMode == mode) return;
    m_toolMode = mode;
    m_panning  = false;

    if (mode == ToolMode::Pan)
        setCursor(Qt::OpenHandCursor);
    else
        setCursor(m_selMode == SelectionMode::TextFlowMode
                  ? Qt::IBeamCursor : Qt::CrossCursor);

    emit toolModeChanged(mode);
}

// ─────────────────────────────────────────────────────────────────────────────
// copyToClipboard
// ─────────────────────────────────────────────────────────────────────────────
void PdfCanvasView::copyToClipboard()
{
    if (!m_selectedText.isEmpty())
        QApplication::clipboard()->setText(m_selectedText);
}
