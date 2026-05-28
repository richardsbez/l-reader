// pdf_text_layer.hpp  —  l-reader
//
// Camada de manipulação de texto PDF: seleção, highlights, busca e clipboard.
// Completamente desacoplada do layout/scroll do widget pai.
//
// Uso:
//   1. Construir com duas lambdas que descrevem o sistema de coordenadas:
//        • pageRectFn(i) → QRect do widget para a página i
//        • scaleFn(i)    → razão pixels/pt da página i
//      Em PdfCanvasView a escala é global (BASE_DPI/72 × zoom).
//      Em CasualPdfView varia por página (fit-to-area).
//
//   2. Chamar setDocument() ao abrir um documento.
//
//   3. Conectar repaintNeeded/repaintAll ao update() do widget pai.
//
//   4. Delegar eventos de mouse/teclado com os métodos handle*().
//
//   5. Chamar paintOverlays() no final do paintEvent() do widget pai.

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
  // Retorna o rect em pixels (coords do widget) da página i.
  // Rect inválido/nulo = página fora da área visível.
  using PageRectFn = std::function<QRect(int pageIndex)>;

  // Retorna a escala de renderização: pixels por ponto PDF da página i.
  // PdfCanvasView passa uma lambda que ignora o índice (escala global).
  // CasualPdfView passa uma lambda que calcula por página (fit-to-area).
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
  // ptRects em coordenadas de pontos PDF (mesma unidade dos highlights).
  void setSearchHighlights(int page, const QList<QRectF> &ptRects);
  void clearSearchHighlights();

  // ── Delegação de eventos ──────────────────────────────────────────────
  // Retornam true se o evento foi consumido (caller chama event->accept()).
  [[nodiscard]] bool handleMousePress(QMouseEvent *e);
  [[nodiscard]] bool handleMouseMove(QMouseEvent *e);
  [[nodiscard]] bool handleMouseRelease(QMouseEvent *e);
  [[nodiscard]] bool handleKeyPress(QKeyEvent *e);
  void handleContextMenu(QContextMenuEvent *e, QWidget *viewport);

  // ── Pintura ───────────────────────────────────────────────────────────
  // Chamar ao final de paintEvent() do widget pai, após renderizar páginas.
  // Pinta: highlights permanentes → busca → seleção → rubber-band.
  void paintOverlays(QPainter &p, const QRect &clip) const;

  // ── Cursor sugerido para o modo atual ─────────────────────────────────
  [[nodiscard]] Qt::CursorShape suggestedCursor() const noexcept;

signals:
  void textSelected(const QString &text);
  void highlightRequested(HighlightEntry entry);
  void removeHighlightRequested(QString id);
  void toolModeChanged(ToolMode mode);
  void selectionModeChanged(SelectionMode mode);

  // O widget pai conecta estes ao seu update() / update(rect)
  void repaintNeeded(QRect region); ///< repaint parcial (mais eficiente)
  void repaintAll();                ///< repaint completo do widget

  void cursorChanged(Qt::CursorShape shape);

private:
  // ── Tipos internos ────────────────────────────────────────────────────
  struct WordInfo {
    QString text;
    QRectF bbox;
    bool hasSpaceAfter = false;
    QVector<QRectF> charBoxes; ///< uma entry por char (pode ter isEmpty())
  };

  struct PageSelection {
    int page;
    QVector<QRectF> ptRects; ///< rects em coordenadas de pontos PDF
  };

  // ── Word cache ────────────────────────────────────────────────────────
  // Evita re-chamar pg->textList() a cada mouseMoveEvent.
  // Custo unitário = número de palavras; limite total kWordCacheMaxCost.
  static constexpr int kWordCacheMaxCost = 15'000; // ~50 págs × 300 palavras
  QCache<int, QVector<WordInfo>> m_wordCache{kWordCacheMaxCost};

  [[nodiscard]] QVector<WordInfo> getPageWords(int page);
  static void ensureMergedRects(HighlightEntry &h);

  // ── Conversão de coordenadas ──────────────────────────────────────────
  bool widgetPointToPagePt(const QPoint &wp, int *outPage, qreal *outPtX,
                           qreal *outPtY) const;
  [[nodiscard]] QRect selectionBoundingRectWidget() const;

  // ── Seleção ───────────────────────────────────────────────────────────
  void computeSelection();         ///< RectMode  — chamado em mouseRelease
  void computeTextFlowSelection(); ///< TextFlowMode — chamado pelo throttle

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

  // Throttle de seleção (16 ms ≈ 60 fps) — evita pg->textList() a cada pixel
  QTimer *m_selThrottle = nullptr;
  bool m_selPending = false;

  // Highlights permanentes
  QVector<HighlightEntry> m_highlights;

  // Search highlights temporários
  int m_searchHlPage = -1;
  QList<QRectF> m_searchHlRects;

  static constexpr qreal kHlCornerRadius = 2.5;
};
