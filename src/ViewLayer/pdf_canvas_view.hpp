// pdf_canvas_view.hpp
#pragma once
#include <QWidget>
#include <QVector>
#include <QScrollArea>
#include <poppler-qt6.h>
#include "RenderSubsystem/page_cache.hpp"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>

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
        Pan             ///< arrastar para navegar (mãozinha)
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

public slots:
    void requestRepaintPage(int page);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void toggleSelectionMode();
    void copyToClipboard();
    void clearSelection();

signals:
    void currentPageChanged(int page);
    void zoomChanged(qreal zoom);
    void textSelected(const QString& text);
    void selectionModeChanged(PdfCanvasView::SelectionMode mode);
    void toolModeChanged(PdfCanvasView::ToolMode mode);

protected:
    void paintEvent       (QPaintEvent*        event) override;
    void keyPressEvent    (QKeyEvent*           event) override;
    void resizeEvent      (QResizeEvent*        event) override;
    void mousePressEvent  (QMouseEvent*         event) override;
    void mouseMoveEvent   (QMouseEvent*         event) override;
    void mouseReleaseEvent(QMouseEvent*         event) override;
    void contextMenuEvent (QContextMenuEvent*   event) override;
    void wheelEvent       (QWheelEvent*         event) override { event->ignore(); }

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
    void syncCurrentPage(int scrollY);

    // ── Text selection helpers ─────────────────────────────────────────────
    void computeSelection();          // RectMode
    void computeTextFlowSelection();  // TextFlowMode

    [[nodiscard]] QRect pageRectInWidget(int i) const;
    bool widgetPointToPagePt(const QPoint& wp,
                             int*   outPage,
                             qreal* outPtX,
                             qreal* outPtY) const;

    // ── Inner types ───────────────────────────────────────────────────────
    struct PageEntry {
        int top, width, height;
    };

    // RectMode  → 1 rect (intersecção retangular)
    // TextFlowMode → N rects, um por palavra
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

    // ── Selection state ───────────────────────────────────────────────────
    bool                   m_selecting    = false;
    bool                   m_hasSelection = false;
    QPoint                 m_selOrigin;
    QPoint                 m_selCurrent;
    QVector<PageSelection> m_pageSelections;
    QString                m_selectedText;
    SelectionMode          m_selMode = SelectionMode::TextFlowMode;

    // ── Pan state ─────────────────────────────────────────────────────────
    ToolMode m_toolMode  = ToolMode::Select;
    bool     m_panning   = false;
    QPoint   m_panOrigin;

    // ── Constants ─────────────────────────────────────────────────────────
    static constexpr int   PAGE_GAP  = 20;
    static constexpr qreal BASE_DPI  = 150.0;
    static constexpr qreal ZOOM_MIN  = 0.25;
    static constexpr qreal ZOOM_MAX  = 4.00;
    static constexpr qreal ZOOM_STEP = 0.15;
};
