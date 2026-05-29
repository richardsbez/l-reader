// casual_pdf_view.hpp  —  l-reader
//
// Widget de leitura "book spread" para o Modo Casual (PDF).
//
// Extensões em relação à versão original:
//   • Aceita Poppler::Document* em setPageCache() para habilitar extração de
//   texto. • Contém PdfTextLayer: seleção de texto, highlights e busca
//   funcionam no spread. • Interação: drag = seleção de texto; click simples
//   (sem drag) = virar página. • Expõe a mesma API de highlights que
//   PdfCanvasView — MainWindow pode
//     conectar os mesmos signals/slots de HighlightManager.

#pragma once

#include "CasualMode/casual_mode_controller.hpp"
#include "DocumentEngine/highlight_entry.hpp"
#include "RenderSubsystem/page_cache.hpp"
#include "ViewLayer/pdf_text_layer.hpp"
#include <QPixmap>
#include <QPropertyAnimation>
#include <QQuickWidget>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <memory>
#include <optional>
#include <poppler-qt6.h>

class CasualPdfView final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
  explicit CasualPdfView(QWidget *parent = nullptr);

  // doc: necessário para extração de texto. Pode ser nullptr para desabilitar
  // texto.
  void setPageCache(PageCache *cache, Poppler::Document *doc, int pageCount);

  void goToSpread(int leftPage);
  void nextSpread();
  void prevSpread();

  [[nodiscard]] int currentLeftPage() const { return m_leftPage; }
  [[nodiscard]] int pageCount() const { return m_pageCount; }

  void setBackgroundColor(const QColor &c);

  // Liga o CasualModeController ao footer QML.
  // Deve ser chamado antes de goToSpread() — normalmente em onModeChanged().
  void setController(CasualModeController *ctrl);

  // Altura reservada pelo footer QML (usado para calcular a área útil).
  static constexpr int kFooterHeight = 44;

  void activateDpi();
  void deactivateDpi();

  // ── API de texto — espelha PdfCanvasView ─────────────────────────────
  void addHighlight(const HighlightEntry &h);
  void removeHighlight(const QString &id);
  void clearHighlights();
  void setSearchHighlights(int page, const QList<QRectF> &ptRects);
  void clearSearchHighlights();
  void copyToClipboard();
  void clearSelection();

  void setSelectionMode(PdfTextLayer::SelectionMode m);
  void setToolMode(PdfTextLayer::ToolMode mode);

signals:
  void spreadChanged(int leftPage, int rightPage);

  // ── Sinais de texto — conectar ao HighlightManager ───────────────────
  void textSelected(const QString &text);
  void highlightRequested(HighlightEntry entry);
  void removeHighlightRequested(QString id);

public slots:
  void onPageReady(int page);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

private:
  void requestCurrentPages();
  void prefetchAdjacentSpreads();
  void scheduleFadeIn();
  qreal computeDpi() const;

  [[nodiscard]] QRect leftPageRect() const;
  [[nodiscard]] QRect rightPageRect() const;
  [[nodiscard]] QRect pageRectForIndex(int i) const;
  [[nodiscard]] qreal scaleForPage(int i) const;
  void drawSpinner(QPainter &p, const QRect &area) const;

  qreal opacity() const { return m_opacity; }
  void setOpacity(qreal v) {
    m_opacity = v;
    update();
  }

  // ── Document ──────────────────────────────────────────────────────────
  PageCache *m_cache = nullptr;
  Poppler::Document *m_doc = nullptr;
  int m_pageCount = 0;
  int m_leftPage = 0;
  QVector<QSizeF> m_pageSizes; ///< pré-carregados em setPageCache()

  // ── Footer QML overlay ────────────────────────────────────────────────
  QQuickWidget *m_footerWidget = nullptr;
  CasualModeController *m_controller = nullptr;
  void repositionFooter();

  // ── Rendering ────────────────────────────────────────────────────────
  QColor m_bg{0xF5, 0xF5, 0xF5};
  std::optional<QPixmap> m_leftPx;
  std::optional<QPixmap> m_rightPx;

  qreal m_opacity = 1.0;
  QPropertyAnimation *m_fadeAnim = nullptr;

  QTimer *m_spinnerTimer = nullptr;
  QTimer *m_spinnerDelayTimer = nullptr; // v2: adia exibição do spinner
  bool m_showSpinner = false;            // v2: spinner só visível após delay
  int m_spinnerAngle = 0;
  qreal m_activeDpi = 0.0;

  // Delay em ms antes de mostrar o spinner — evita flash em páginas rápidas.
  static constexpr int kSpinnerDelayMs = 90;

  // ── Interação de texto ────────────────────────────────────────────────
  std::unique_ptr<PdfTextLayer> m_textLayer;

  // ── Constantes ────────────────────────────────────────────────────────
  static constexpr int kGutterPx = 24;
  static constexpr int kPageMarginPx = 32;
  static constexpr int kShadowPx = 5;
  static constexpr qreal kCasualDpi =
      130.0; // v2: +18% resolução — texto mais nítido
};
