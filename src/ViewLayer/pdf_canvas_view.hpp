// pdf_canvas_view.hpp
#pragma once
#include "DocumentEngine/highlight_entry.hpp"
#include "RenderSubsystem/page_cache.hpp"
#include "ViewLayer/pdf_text_layer.hpp"
#include <QScrollArea>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>
#include <memory>
#include <poppler-qt6.h>

class PdfCanvasView final : public QWidget {
  Q_OBJECT
public:
  // ── Modos — re-exportados para compatibilidade com MainWindow ─────────
  // Os valores espelham PdfTextLayer::SelectionMode/ToolMode; tradução
  // é feita internamente nos setters. Pan não existe na camada de texto.
  enum class SelectionMode { RectMode, TextFlowMode };
  Q_ENUM(SelectionMode)

  enum class ToolMode { Select, Pan, Annotate };
  Q_ENUM(ToolMode)

  explicit PdfCanvasView(QWidget *parent = nullptr);

  void setDocument(PageCache *cache, Poppler::Document *doc, int pageCount);
  void goToPage(int page);

  [[nodiscard]] int currentPage() const { return m_currentPage; }
  [[nodiscard]] int pageCount() const { return m_pageCount; }
  [[nodiscard]] qreal zoom() const { return m_zoom; }
  [[nodiscard]] SelectionMode selectionMode() const;
  [[nodiscard]] ToolMode toolMode() const { return m_toolMode; }

  void setSelectionMode(SelectionMode m);
  void setToolMode(ToolMode mode);

  void onScrollChanged(int scrollY);

  [[nodiscard]] QSize sizeHint() const override;

  // Zoom ancorado a um ponto Y da viewport (Ctrl+roda do eventFilter).
  void zoomAround(qreal newZoom, int viewportAnchorY);

  // Zoom multiplicativo acumulando sobre o alvo da animação.
  void applyZoomFactor(qreal factor, int viewportAnchorY);

public slots:
  void requestRepaintPage(int page);
  void zoomIn();
  void zoomOut();
  void zoomReset();
  void toggleSelectionMode();
  void copyToClipboard();
  void clearSelection();

  // ── Highlights ────────────────────────────────────────────────────────
  void addHighlight(const HighlightEntry &h);
  void removeHighlight(const QString &id);
  void clearHighlights();
  [[nodiscard]] QString highlightIdAtPoint(const QPoint &widgetPos) const;

  // ── Search highlights (temporários) ───────────────────────────────────
  void setSearchHighlights(int page, const QList<QRectF> &ptRects);
  void clearSearchHighlights();

  // ── Cor ativa de highlight ─────────────────────────────────────────────
  void setHighlightColor(const QColor &color);
  [[nodiscard]] QColor highlightColor() const;

signals:
  void currentPageChanged(int page);
  void zoomChanged(qreal zoom);
  void textSelected(const QString &text);
  void selectionModeChanged(PdfCanvasView::SelectionMode mode);
  void toolModeChanged(PdfCanvasView::ToolMode mode);
  void highlightRequested(HighlightEntry entry);
  void removeHighlightRequested(QString id);

protected:
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

  // ── Constantes (usadas pelo eventFilter do MainWindow) ────────────────
  static constexpr int PAGE_GAP = 20;
  static constexpr qreal BASE_DPI = 150.0;
  static constexpr qreal ZOOM_MIN = 0.25;
  static constexpr qreal ZOOM_MAX = 4.00;
  static constexpr qreal ZOOM_STEP = 0.15;

private:
  [[nodiscard]] QScrollArea *scrollArea() const {
    QWidget *vp = parentWidget();
    return vp ? qobject_cast<QScrollArea *>(vp->parentWidget()) : nullptr;
  }
  [[nodiscard]] int viewportHeight() const {
    QWidget *vp = parentWidget();
    return vp ? vp->height() : 600;
  }

  void rebuildLayout();
  void applyZoom(qreal newZoom);
  void zoomAround(qreal newZoom, int viewportAnchorY, bool animate);
  void syncCurrentPage(int scrollY);

  [[nodiscard]] QRect pageRectInWidget(int i) const;

  // ── Layout ────────────────────────────────────────────────────────────
  struct PageEntry {
    int top, width, height;
  };

  // ── Zoom animation ─────────────────────────────────────────────────────
  QVariantAnimation *m_zoomAnim = nullptr;
  qreal m_zoomAnchorFrac = 0.0;
  int m_zoomAnchorScreenY = 0;

  // ── Document state ────────────────────────────────────────────────────
  PageCache *m_cache = nullptr;
  Poppler::Document *m_doc = nullptr;
  int m_pageCount = 0;
  int m_currentPage = 0;
  qreal m_zoom = 1.0;
  int m_scrollY = 0;

  QVector<PageEntry> m_layout;
  int m_totalHeight = 0;
  QVector<QSizeF> m_pageSizes; ///< pré-carregados em setDocument()

  // ── Pan state (não vai para PdfTextLayer — específico do scroll view) ─
  ToolMode m_toolMode = ToolMode::Select;
  bool m_panning = false;
  QPoint m_panOrigin;

  // ── Camada de texto ───────────────────────────────────────────────────
  std::unique_ptr<PdfTextLayer> m_textLayer;
};
