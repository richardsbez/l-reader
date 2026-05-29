// page_cache.hpp  —  l-reader
//
// Cache LRU de páginas PDF com pré-renderização agressiva e duas filas
// de prioridade (principal / galeria).
//
// Melhorias v2:
//   • PRE_FORWARD: 5 → 10  — pré-carrega mais à frente para navegação
//                            quase instantânea em leituras contínuas
//   • PRE_BACK: 2 → 4      — segura mais páginas anteriores para retorno rápido
//   • MAX_COST: 100 → 256  — cache maior evita re-render ao voltar
//   • Pool principal agora usa 2 threads dedicadas (era globalInstance)
//     → requests da view nunca competem com outras tarefas Qt
//   • prioritizeRender(page): método urgente que cancela gallery renders
//     in-flight e promove o page para o topo da fila
//   • isReady(page): consulta O(1) sem std::optional
#pragma once
#include "render_worker.hpp"
#include <QCache>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>
#include <optional>

class PageCache final : public QObject {
  Q_OBJECT
public:
  // ── Constantes de pré-carregamento ───────────────────────────────────────
  static constexpr int PRE_BACK = 4; // páginas antes da atual em cache
  static constexpr int PRE_FORWARD =
      10;                              // páginas após a atual pré-renderizadas
  static constexpr int MAX_COST = 256; // MB máximos em cache LRU

  explicit PageCache(QObject *parent = nullptr);
  ~PageCache() override;

  void setEngine(Poppler::Document *doc, int pageCount);

  // Atualiza o DPI de renderização e invalida o cache inteiro.
  void setDpi(qreal dpi);

  // Retorna o pixmap se estiver em cache, nullopt caso contrário.
  [[nodiscard]] std::optional<QPixmap> get(int page) const;

  // true se a página está em cache e pronta para exibição imediata.
  [[nodiscard]] bool isReady(int page) const;

  // Chamado pelo CasualPdfView/PdfCanvasView ao mudar de página.
  // Dispara pré-renderização da janela [page-PRE_BACK, page+PRE_FORWARD].
  void onCurrentPageChanged(int page);

  // Chamado pela navegação principal — pool de alta prioridade.
  void requestRender(int page);

  // Urgente: evita mostrar spinner quando a página está a 1 salto.
  // Emite pageReady() logo que o render termina, sem esperar a janela normal.
  void prioritizeRender(int page);

  // Chamado pela Galeria — pool separado de baixa prioridade.
  void requestGalleryRender(int page);

  // Invalida e remove uma página específica do cache (ex: após zoom).
  void invalidatePage(int page);

signals:
  // Emitido na UI thread quando uma página termina de renderizar.
  void pageReady(int page);

private slots:
  void onPageRendered(int page, QImage image, int generation);

private:
  void scheduleRender(int page, QThreadPool *pool, int priority = 0);
  [[nodiscard]] bool isInFlight(int page) const;

  QCache<int, QPixmap> m_cache; // LRU built-in do Qt
  QSet<int> m_inFlight;         // páginas com render em andamento

  // Pool principal: 2 threads dedicadas, prioridade Normal.
  // Separado do globalInstance para não competir com tarefas do Qt.
  QThreadPool *m_pool;

  // Pool da galeria: 1 thread, prioridade mínima.
  QThreadPool *m_galleryPool;

  Poppler::Document *m_doc = nullptr;
  int m_count = 0;
  qreal m_dpi = 150.0;
  int m_generation = 0;
};
