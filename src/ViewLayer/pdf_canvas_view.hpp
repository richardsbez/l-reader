// pdf_canvas_view.hpp
#pragma once
#include <QWidget>
#include <QVector>
#include <QHash>
#include <QCache>
#include <QScrollArea>
#include <QTimer>
#include <poppler-qt6.h>
#include "RenderSubsystem/page_cache.hpp"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QVariantAnimation>

// ─────────────────────────────────────────────────────────────────────────────
// HighlightEntry — tipos compartilhados com HighlightManager
// ─────────────────────────────────────────────────────────────────────────────
#include "DocumentEngine/highlight_entry.hpp"

class PdfCanvasView final : public QWidget {
    Q_OBJECT
public:
    // ── Selection mode ────────────────────────────────────────────────────
    // DEVE vir antes do bloco signals: para que o MOC conheça o tipo.
    enum class SelectionMode {
        RectMode,       ///< rubber-band → extrai texto da área retangular
        TextFlowMode    ///< seleciona palavra por palavra seguindo o fluxo
    };
    Q_ENUM(SelectionMode)

    // ── Tool mode ─────────────────────────────────────────────────────────
    enum class ToolMode {
        Select,         ///< seleção de texto (comportamento padrão)
        Pan,            ///< arrastar para navegar (mãozinha)
        Annotate        ///< marca-texto: seleciona e cria highlight amarelo
    };
    Q_ENUM(ToolMode)

    explicit PdfCanvasView(QWidget* parent = nullptr);

    void setDocument(PageCache* cache, Poppler::Document* doc, int pageCount);
    void goToPage(int page);

    [[nodiscard]] int          currentPage()    const { return m_currentPage; }
    [[nodiscard]] int          pageCount()      const { return m_pageCount;   }
    [[nodiscard]] qreal        zoom()           const { return m_zoom;        }
    [[nodiscard]] SelectionMode selectionMode() const { return m_selMode;     }
    [[nodiscard]] ToolMode      toolMode()      const { return m_toolMode;    }

    void setSelectionMode(SelectionMode m);
    void setToolMode(ToolMode mode);

    void onScrollChanged(int scrollY);

    [[nodiscard]] QSize sizeHint() const override;

    // Zoom ancorado a um ponto Y da viewport — o conteúdo sob o cursor permanece fixo.
    // Usado pelo Ctrl+roda no eventFilter do MainWindow.
    void zoomAround(qreal newZoom, int viewportAnchorY);

    // Aplica um fator multiplicativo ao zoom atual, acumulando sobre o alvo
    // da animação em andamento (garante suavidade em Ctrl+roda rápido).
    void applyZoomFactor(qreal factor, int viewportAnchorY);

public slots:
    void requestRepaintPage(int page);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void toggleSelectionMode();
    void copyToClipboard();
    void clearSelection();

    // ── Highlights (modo Anotar) ───────────────────────────────────────────
    void addHighlight(const HighlightEntry& h);
    void removeHighlight(const QString& id);
    void clearHighlights();
    [[nodiscard]] QString highlightIdAtPoint(const QPoint& widgetPos) const;

    // ── Search highlights (temporários — limpados a cada nova busca) ──────
    // ptRects estão em coordenadas de pontos PDF (mesmas unidades dos highlights).
    void setSearchHighlights(int page, const QList<QRectF>& ptRects);
    void clearSearchHighlights();

signals:
    void currentPageChanged(int page);
    void zoomChanged(qreal zoom);
    void textSelected(const QString& text);
    void selectionModeChanged(PdfCanvasView::SelectionMode mode);
    void toolModeChanged(PdfCanvasView::ToolMode mode);
    /// Emitido quando o usuário termina uma seleção no modo Annotate.
    void highlightRequested(HighlightEntry entry);
    /// Emitido quando o usuário clica com botão direito sobre um highlight.
    void removeHighlightRequested(QString id);

protected:
    void paintEvent       (QPaintEvent*        event) override;
    void keyPressEvent    (QKeyEvent*           event) override;
    void resizeEvent      (QResizeEvent*        event) override;
    void mousePressEvent  (QMouseEvent*         event) override;
    void mouseMoveEvent   (QMouseEvent*         event) override;
    void mouseReleaseEvent(QMouseEvent*         event) override;
    void contextMenuEvent (QContextMenuEvent*   event) override;
    void wheelEvent       (QWheelEvent*         event) override { event->ignore(); }

    // ── Constants ─────────────────────────────────────────────────────────
    // Públicos para que o eventFilter do MainWindow possa usá-los no clamp
    // de zoom antes de chamar applyZoomFactor.
    static constexpr int   PAGE_GAP  = 20;
    static constexpr qreal BASE_DPI  = 150.0;
    static constexpr qreal ZOOM_MIN  = 0.25;
    static constexpr qreal ZOOM_MAX  = 4.00;
    static constexpr qreal ZOOM_STEP = 0.15;

private:
    [[nodiscard]] QScrollArea* scrollArea() const {
        QWidget* vp = parentWidget();
        return vp ? qobject_cast<QScrollArea*>(vp->parentWidget()) : nullptr;
    }
    [[nodiscard]] int viewportHeight() const {
        QWidget* vp = parentWidget();
        return vp ? vp->height() : 600;
    }

    void rebuildLayout();
    void applyZoom(qreal newZoom);

    // ── Zoom animation ─────────────────────────────────────────────────────
    // Anima a transição de m_zoom de forma suave. Separar o zoom visual
    // (atualizado a cada frame) do zoom de renderização (atualizado só quando
    // o zoom para, via sinal zoomChanged + debounce no MainWindow) permite que
    // o layout flua sem invalidar o cache a cada frame intermediário.
    void zoomAround(qreal newZoom, int viewportAnchorY, bool animate);

    QVariantAnimation* m_zoomAnim          = nullptr;
    qreal              m_zoomAnchorFrac    = 0.0;  ///< fração de m_totalHeight a manter fixo
    int                m_zoomAnchorScreenY = 0;    ///< y na viewport a manter fixo (px)
    void syncCurrentPage(int scrollY);

    // ── Text selection helpers ─────────────────────────────────────────────
    void computeSelection();          // RectMode
    void computeTextFlowSelection();  // TextFlowMode

    [[nodiscard]] QRect pageRectInWidget(int i) const;
    bool widgetPointToPagePt(const QPoint& wp,
                             int*   outPage,
                             qreal* outPtX,
                             qreal* outPtY) const;

    // Retorna o bounding rect (widget coords) da seleção atual.
    // Usado para update(rect) parcial em vez de update() full-widget.
    [[nodiscard]] QRect selectionBoundingRectWidget() const;

    // ── Word cache ────────────────────────────────────────────────────────
    // Evita re-chamar pg->textList() em todo mouseMoveEvent.
    // Cache é limpo no setDocument() e permanece válido independente do zoom
    // (os rects são em pontos PDF, coordenada-invariante de escala).
    struct WordInfo {
        QString         text;
        QRectF          bbox;
        bool            hasSpaceAfter = false;
        QVector<QRectF> charBoxes;    ///< uma entrada por caractere (pode ter isEmpty())
    };

    // Retorna referência estável ao cache da página (constrói se ausente).
    QVector<WordInfo> getPageWords(int page);

    // Preenche m_highlights[idx].spans[*].mergedPtRects para um highlight.
    // Chamado em addHighlight() e nos highlights carregados do disco que
    // ainda não têm o cache preenchido.
    static void ensureMergedRects(HighlightEntry& h);

    // ── Pintura da seleção — helper interno ───────────────────────────────
    // Factoriza o bloco de highlight azul que antes estava duplicado em
    // paintEvent (uma vez fora do m_selecting, outra dentro).
    void paintSelection(QPainter& p, const QRect& clip) const;

    // ── Inner types ───────────────────────────────────────────────────────
    struct PageEntry {
        int top, width, height;
    };

    // RectMode  → 1 rect (intersecção retangular)
    // TextFlowMode → N rects, um por caractere
    struct PageSelection {
        int             page;
        QVector<QRectF> ptRects;
    };

    // ── Document state ────────────────────────────────────────────────────
    PageCache*         m_cache       = nullptr;
    Poppler::Document* m_doc         = nullptr;
    int                m_pageCount   = 0;
    int                m_currentPage = 0;
    qreal              m_zoom        = 1.0;
    int                m_scrollY     = 0;

    QVector<PageEntry> m_layout;
    int                m_totalHeight = 0;

    // ── Page size cache ───────────────────────────────────────────────────
    // rebuildLayout() precisava chamar m_doc->page(i) para obter o tamanho
    // de cada página. Para documentos com centenas de páginas isso tornava
    // rebuildLayout() lento demais para ser chamado a cada frame de animação.
    // Os tamanhos são populados uma vez em setDocument() e reutilizados.
    QVector<QSizeF>    m_pageSizes;

    // ── Word cache (limpo em setDocument) ────────────────────────────────
    // QCache com limite de kWordCacheMaxCost palavras evita consumo ilimitado
    // em PDFs longos — o QHash original não tinha evicção.
    static constexpr int kWordCacheMaxCost = 15000;  // ~50 pág × 300 palavras
    QCache<int, QVector<WordInfo>> m_wordCache { kWordCacheMaxCost };

    // ── Search highlights (temporários) ──────────────────────────────────
    int            m_searchHlPage = -1;       // página dos resultados ativos
    QList<QRectF>  m_searchHlRects;           // rects em coords de pontos PDF

    // ── Selection state ───────────────────────────────────────────────────
    bool                   m_selecting    = false;
    bool                   m_hasSelection = false;
    QPoint                 m_selOrigin;
    QPoint                 m_selCurrent;
    QVector<PageSelection> m_pageSelections;
    QString                m_selectedText;
    SelectionMode          m_selMode = SelectionMode::TextFlowMode;

    // ── Throttle de seleção (timer de 16 ms ≈ 60 fps) ────────────────────
    // Evita chamar computeTextFlowSelection() a cada pixel arrastado.
    QTimer* m_selThrottle  = nullptr;
    bool    m_selPending   = false;   ///< há posição nova aguardando processamento

    // ── Pan state ─────────────────────────────────────────────────────────
    ToolMode m_toolMode  = ToolMode::Select;
    bool     m_panning   = false;
    QPoint   m_panOrigin;

    // ── Highlights permanentes (modo Anotar) ──────────────────────────────
    QVector<HighlightEntry> m_highlights;

    // ── Raio dos cantos arredondados do highlight (em pixels de tela) ────
    static constexpr qreal HL_CORNER_RADIUS = 2.5;
};
