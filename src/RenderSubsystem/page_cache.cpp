// page_cache.cpp  —  l-reader
// ─────────────────────────────────────────────────────────────────────────────
#include "page_cache.hpp"

#include <QThread>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
PageCache::PageCache(QObject *parent) : QObject(parent), m_cache(MAX_COST) {
  // Pool principal: 2 threads dedicadas.
  // Usando pool próprio evitamos que renders de PDF disputem com tarefas
  // internas do Qt (networking, timers, etc.).
  m_pool = new QThreadPool(this);
  m_pool->setMaxThreadCount(2);

  // Pool da galeria: 1 thread, thumbnails nunca bloqueiam o spread atual.
  m_galleryPool = new QThreadPool(this);
  m_galleryPool->setMaxThreadCount(1);
}

PageCache::~PageCache() {
  // Aguarda workers em voo antes de destruir — evita acesso a m_doc inválido.
  m_pool->waitForDone(500);
  m_galleryPool->waitForDone(200);
}

// ─────────────────────────────────────────────────────────────────────────────
// setEngine
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::setEngine(Poppler::Document *doc, int pageCount) {
  ++m_generation; // invalida workers em voo com geração anterior
  m_doc = doc;
  m_count = pageCount;
  m_cache.clear();
  m_inFlight.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// setDpi — invalida cache inteiro; renders em voo são descartados via
// generation
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::setDpi(qreal dpi) {
  if (qFuzzyCompare(dpi, m_dpi))
    return;
  m_dpi = dpi;
  ++m_generation;
  m_cache.clear();
  m_inFlight.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// get / isReady
// ─────────────────────────────────────────────────────────────────────────────
std::optional<QPixmap> PageCache::get(int page) const {
  if (auto *px = m_cache.object(page))
    return *px;
  return std::nullopt;
}

bool PageCache::isReady(int page) const { return m_cache.contains(page); }

// ─────────────────────────────────────────────────────────────────────────────
// onCurrentPageChanged — janela de pré-renderização expandida
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::onCurrentPageChanged(int page) {
  const int first = std::max(0, page - PRE_BACK);
  const int last = std::min(m_count - 1, page + PRE_FORWARD);

  // Prioridade decrescente: páginas mais próximas da atual têm mais prioridade
  for (int p = first; p <= last; ++p) {
    if (!m_cache.contains(p) && !isInFlight(p)) {
      // Prioridade: distância negativa → páginas próximas são agendadas
      // antes dentro da fila do pool.
      const int distance = std::abs(p - page);
      const int priority = std::max(0, 20 - distance);
      scheduleRender(p, m_pool, priority);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// requestRender — navegação normal (pool principal)
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::requestRender(int page) {
  if (page < 0 || page >= m_count)
    return;
  if (!m_cache.contains(page) && !isInFlight(page))
    scheduleRender(page, m_pool, /*priority=*/10);
}

// ─────────────────────────────────────────────────────────────────────────────
// prioritizeRender — render urgente, máxima prioridade
// Garante que a página apareça o mais rápido possível, mesmo que haja renders
// da galeria enfileirados. Se a página já está em voo, não duplica.
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::prioritizeRender(int page) {
  if (page < 0 || page >= m_count)
    return;
  if (m_cache.contains(page)) {
    // Já em cache — emite imediatamente para que a view atualize
    emit pageReady(page);
    return;
  }
  if (!isInFlight(page))
    scheduleRender(page, m_pool, /*priority=*/20); // topo da fila
}

// ─────────────────────────────────────────────────────────────────────────────
// requestGalleryRender — thumbnails (pool de baixa prioridade)
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::requestGalleryRender(int page) {
  if (page < 0 || page >= m_count)
    return;
  if (!m_cache.contains(page) && !isInFlight(page))
    scheduleRender(page, m_galleryPool, /*priority=*/0);
}

// ─────────────────────────────────────────────────────────────────────────────
// invalidatePage — remove do cache (ex: após mudança de DPI via zoom)
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::invalidatePage(int page) { m_cache.remove(page); }

// ─────────────────────────────────────────────────────────────────────────────
// scheduleRender — agendamento interno com prioridade
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::scheduleRender(int page, QThreadPool *pool, int priority) {
  if (!m_doc)
    return;
  m_inFlight.insert(page);

  auto *worker = new RenderWorker(m_doc, page, m_dpi, m_generation);

  // QueuedConnection: onPageRendered sempre roda na UI thread.
  connect(worker, &RenderWorker::pageRendered, this, &PageCache::onPageRendered,
          Qt::QueuedConnection);

  // Cleanup seguro: deleteLater via event loop, não pela thread pool.
  connect(worker, &RenderWorker::pageRendered, worker, &QObject::deleteLater,
          Qt::QueuedConnection);
  connect(worker, &RenderWorker::renderFailed, worker, &QObject::deleteLater,
          Qt::QueuedConnection);

  pool->start(worker, priority);
}

// ─────────────────────────────────────────────────────────────────────────────
// onPageRendered — slot da UI thread
// ─────────────────────────────────────────────────────────────────────────────
void PageCache::onPageRendered(int page, QImage image, int generation) {
  m_inFlight.remove(page);

  // Descarta renderizações de gerações obsoletas.
  if (generation != m_generation)
    return;

  // QPixmap::fromImage é seguro aqui: estamos na UI thread.
  auto *px = new QPixmap(QPixmap::fromImage(std::move(image)));
  const int cost =
      std::max(1, (px->width() * px->height() * 4) / (1024 * 1024));
  m_cache.insert(page, px, cost);
  emit pageReady(page);
}

// ─────────────────────────────────────────────────────────────────────────────
// isInFlight
// ─────────────────────────────────────────────────────────────────────────────
bool PageCache::isInFlight(int page) const { return m_inFlight.contains(page); }
