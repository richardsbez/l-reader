// page_cache.cpp
#include "page_cache.hpp"

PageCache::PageCache(QObject* parent)
    : QObject(parent)
    , m_cache(MAX_COST)
    , m_pool(QThreadPool::globalInstance())
{
    // Pool exclusivo para galeria: 1 thread, prioridade mínima.
    // Thumbnails nunca preemptam o render das páginas correntes.
    m_galleryPool = new QThreadPool(this);
    m_galleryPool->setMaxThreadCount(1);
}

void PageCache::setEngine(Poppler::Document* doc, int pageCount) {
    ++m_generation;           // invalida workers em voo
    m_doc   = doc;
    m_count = pageCount;
    m_cache.clear();
    m_inFlight.clear();
}

void PageCache::setDpi(qreal dpi) {
    if (qFuzzyCompare(dpi, m_dpi)) return;
    m_dpi = dpi;
    ++m_generation;           // renders em voo com DPI antigo são descartados
    m_cache.clear();
    m_inFlight.clear();       // workers antigos entregarão mas serão ignorados
}

std::optional<QPixmap> PageCache::get(int page) const {
    if (auto* px = m_cache.object(page))
        return *px;
    return std::nullopt;
}

void PageCache::onCurrentPageChanged(int page) {
    // Janela de pré-renderização: [page-PRE_BACK, page+PRE_FORWARD]
    const int first = std::max(0, page - PRE_BACK);
    const int last  = std::min(m_count - 1, page + PRE_FORWARD);

    for (int p = first; p <= last; ++p) {
        if (!m_cache.contains(p) && !isInFlight(p))
            scheduleRender(p, m_pool);
    }
}

void PageCache::requestRender(int page) {
    if (page < 0 || page >= m_count) return;
    if (!m_cache.contains(page) && !isInFlight(page))
        scheduleRender(page, m_pool);
}

void PageCache::requestGalleryRender(int page) {
    if (page < 0 || page >= m_count) return;
    if (!m_cache.contains(page) && !isInFlight(page))
        scheduleRender(page, m_galleryPool);
}

void PageCache::scheduleRender(int page, QThreadPool* pool) {
    if (!m_doc) return;
    m_inFlight.insert(page);

    auto* worker = new RenderWorker(m_doc, page, m_dpi, m_generation);

    // QueuedConnection garante que onPageRendered rode na UI thread,
    // independente de qual worker thread emitiu o sinal.
    connect(worker, &RenderWorker::pageRendered,
            this,   &PageCache::onPageRendered,
            Qt::QueuedConnection);

    // CORREÇÃO: setAutoDelete(false) está ativo no RenderWorker para evitar
    // que a thread pool delete um QObject fora da UI thread (undefined behavior).
    // Em compensação, agendamos deleteLater() via QueuedConnection.
    connect(worker, &RenderWorker::pageRendered,
            worker, &QObject::deleteLater,
            Qt::QueuedConnection);
    connect(worker, &RenderWorker::renderFailed,
            worker, &QObject::deleteLater,
            Qt::QueuedConnection);

    pool->start(worker);
}

void PageCache::onPageRendered(int page, QImage image, int generation) {
    // Estamos de volta na UI thread — QPixmap::fromImage é seguro.
    m_inFlight.remove(page);

    // Descarta renderizações de gerações obsoletas (DPI ou documento anterior).
    if (generation != m_generation) return;

    auto* px = new QPixmap(QPixmap::fromImage(std::move(image)));
    const int cost = (px->width() * px->height() * 4) / (1024 * 1024); // MB
    m_cache.insert(page, px, cost);
    emit pageReady(page);
}

bool PageCache::isInFlight(int page) const {
    return m_inFlight.contains(page);
}
