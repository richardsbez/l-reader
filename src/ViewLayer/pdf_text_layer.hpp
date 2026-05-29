// pdf_text_layer.hpp  —  l-reader
//
// Camada de manipulação de texto PDF: seleção, highlights, busca e clipboard.
// Completamente desacoplada do layout/scroll do widget pai.
//
// Melhorias v2:
//   • Duplo-clique  → seleciona a palavra sob o cursor
//   • Triplo-clique → seleciona a linha inteira
//   • Shift+click   → estende a seleção existente
//   • Ctrl+H        → cria highlight da seleção com a cor ativa
//   • Menu de contexto com submenu "Destacar como" (4 cores)
//   • setHighlightColor() / highlightColor() para seletor de cor externo
//   • Rendering de seleção melhorado (borda sutil, rubber-band polido)

#pragma once

#include "DocumentEngine/highlight_entry.hpp"
#include <QCache>
#include <QList>
#include <QObject>
#include <QRect>
#include <QTimer>
#include <QVector>
#include <functional>
#include <poppler-qt6.h>

class QPainter;
class QMouseEvent;
class QKeyEvent;
class QContextMenuEvent;
class QWidget;

class PdfTextLayer final : public QObject {
  Q_OBJECT
public:
  // ── Tipos de coordenadas injetadas pelo widget pai ────────────────────
  using PageRectFn = std::function<QRect(int pageIndex)>;
  using ScaleFn = std::function<qreal(int pageIndex)>;

  // ── Modos ─────────────────────────────────────────────────────────────
  enum class SelectionMode {
    RectMode,    ///< rubber-band → extrai texto da área retangular
    TextFlowMode ///< seleciona por fluxo (word-by-word com char boxes)
  };
  Q_ENUM(SelectionMode)

  enum class ToolMode {
    Select,  ///< seleção de texto (padrão)
    Annotate ///< marca-texto: converte seleção em highlight ao soltar
  };
  Q_ENUM(ToolMode)

  explicit PdfTextLayer(PageRectFn pageRectFn, ScaleFn scaleFn,
                        QObject *parent = nullptr);

  // ── Ciclo de vida ─────────────────────────────────────────────────────
  void setDocument(Poppler::Document *doc, int pageCount);
  void clear();

  // ── Modos ─────────────────────────────────────────────────────────────
  [[nodiscard]] SelectionMode selectionMode() const noexcept {
    return m_selMode;
  }
  [[nodiscard]] ToolMode toolMode() const noexcept { return m_toolMode; }
  void setSelectionMode(SelectionMode m);
  void setToolMode(ToolMode mode);
  void toggleSelectionMode();

  // ── Cor ativa de highlight ────────────────────────────────────────────
  // Usada por Ctrl+H, pelo modo Annotate ao soltar, e pelo menu de contexto
  // quando nenhuma cor específica é escolhida.
  void setHighlightColor(const QColor &color);
  [[nodiscard]] QColor highlightColor() const noexcept {
    return m_activeHlColor;
  }

  // ── Seleção de texto ──────────────────────────────────────────────────
  [[nodiscard]] bool hasSelection() const noexcept { return m_hasSelection; }
  [[nodiscard]] QString selectedText() const noexcept { return m_selectedText; }
  void clearSelection();
  void copyToClipboard();

  // ── Highlights permanentes ────────────────────────────────────────────
  void addHighlight(const HighlightEntry &h);
  void removeHighlight(const QString &id);
  void clearHighlights();
  [[nodiscard]] const QVector<HighlightEntry> &highlights() const noexcept {
    return m_highlights;
  }
  [[nodiscard]] QString highlightIdAtPoint(const QPoint &widgetPos) const;

  // ── Highlights de busca (temporários) ────────────────────────────────
  void setSearchHighlights(int page, const QList<QRectF> &ptRects);
  void clearSearchHighlights();

  // ── Delegação de eventos ──────────────────────────────────────────────
  [[nodiscard]] bool handleMousePress(QMouseEvent *e);
  [[nodiscard]] bool handleMouseMove(QMouseEvent *e);
  [[nodiscard]] bool handleMouseRelease(QMouseEvent *e);
  [[nodiscard]] bool handleKeyPress(QKeyEvent *e);
  void handleContextMenu(QContextMenuEvent *e, QWidget *viewport);

  // ── Pintura ───────────────────────────────────────────────────────────
  void paintOverlays(QPainter &p, const QRect &clip) const;

  // ── Cursor sugerido para o modo atual ─────────────────────────────────
  [[nodiscard]] Qt::CursorShape suggestedCursor() const noexcept;

signals:
  void textSelected(const QString &text);
  void highlightRequested(HighlightEntry entry);
  void removeHighlightRequested(QString id);
  void toolModeChanged(ToolMode mode);
  void selectionModeChanged(SelectionMode mode);
  void highlightColorChanged(QColor color);

  void repaintNeeded(QRect region);
  void repaintAll();
  void cursorChanged(Qt::CursorShape shape);

private:
  // ── Tipos internos ────────────────────────────────────────────────────
  struct WordInfo {
    QString text;
    QRectF bbox;
    bool hasSpaceAfter = false;
    QVector<QRectF> charBoxes;
  };

  struct PageSelection {
    int page;
    QVector<QRectF> ptRects;
  };

  // ── Word cache ────────────────────────────────────────────────────────
  static constexpr int kWordCacheMaxCost = 15'000;
  QCache<int, QVector<WordInfo>> m_wordCache{kWordCacheMaxCost};

  [[nodiscard]] QVector<WordInfo> getPageWords(int page);
  static void ensureMergedRects(HighlightEntry &h);

  // ── Conversão de coordenadas ──────────────────────────────────────────
  bool widgetPointToPagePt(const QPoint &wp, int *outPage, qreal *outPtX,
                           qreal *outPtY) const;
  [[nodiscard]] QRect selectionBoundingRectWidget() const;

  // ── Seleção ───────────────────────────────────────────────────────────
  void computeSelection();         ///< RectMode
  void computeTextFlowSelection(); ///< TextFlowMode (chamado pelo throttle)

  /// Seleciona a palavra sob widgetPos (duplo-clique)
  void selectWordAt(const QPoint &widgetPos);

  /// Seleciona a linha inteira sob widgetPos (triplo-clique)
  void selectLineAt(const QPoint &widgetPos);

  /// Cria um HighlightEntry a partir da seleção atual com a cor dada
  void createHighlightFromSelection(const QColor &color);

  // ── Pintura auxiliar ──────────────────────────────────────────────────
  void paintSelection(QPainter &p, const QRect &clip) const;

  // ── Estado ────────────────────────────────────────────────────────────
  Poppler::Document *m_doc = nullptr;
  int m_pageCount = 0;

  PageRectFn m_pageRectFn;
  ScaleFn m_scaleFn;

  // Seleção de texto
  bool m_selecting = false;
  bool m_hasSelection = false;
  QPoint m_selOrigin;
  QPoint m_selCurrent;
  QVector<PageSelection> m_pageSelections;
  QString m_selectedText;
  SelectionMode m_selMode = SelectionMode::TextFlowMode;
  ToolMode m_toolMode = ToolMode::Select;

  // Throttle de seleção (16 ms ≈ 60 fps)
  QTimer *m_selThrottle = nullptr;
  bool m_selPending = false;

  // Multi-click (duplo/triplo)
  QTimer *m_clickTimer = nullptr;
  int m_clickCount = 0;
  QPoint m_lastClickPos;
  static constexpr int kMultiClickMs = 350; // ms entre cliques
  static constexpr int kClickProximity = 6; // px de tolerância

  // Cor ativa de highlight
  QColor m_activeHlColor{255, 220, 0, 150};

  // Highlights permanentes
  QVector<HighlightEntry> m_highlights;

  // Search highlights temporários
  int m_searchHlPage = -1;
  QList<QRectF> m_searchHlRects;

  static constexpr qreal kHlCornerRadius = 2.5;
};
