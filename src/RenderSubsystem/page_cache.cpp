// page_cache.cpp
#include "page_cache.hpp"

PageCache::PageCache(QObject* parent)
    : QObject(parent)
    , m_cache(MAX_COST)
    , m_pool(QThreadPool::globalInstance())
{}

void PageCache::setEngine(Poppler::Document* doc, int pageCount) {
    m_doc   = doc;
    m_count = pageCount;
    m_cache.clear();
    m_inFlight.clear();
}

std::optional<QPixmap> PageCache::get(int page) const {
    if (auto* px = m_cache.object(page))
        return *px;
    return std::nullopt;
}

void PageCache::onCurrentPageChanged(int page) {
    // Janela de pré-renderização: [page-2, page+5]
    const int first = std::max(0, page - PRE_BACK);
    const int last  = std::min(m_count - 1, page + PRE_FORWARD);

    for (int p = first; p <= last; ++p) {
        if (!m_cache.contains(p) && !isInFlight(p))
            scheduleRender(p);
    }
}

void PageCache::scheduleRender(int page) {
    if (!m_doc) return;
    m_inFlight.insert(page);

    auto* worker = new RenderWorker(m_doc, page, m_dpi);

    // QueuedConnection garante que onPageRendered rode na UI thread,
    // independente de qual worker thread emitiu o sinal.
    connect(worker, &RenderWorker::pageRendered,
            this,   &PageCache::onPageRendered,
            Qt::QueuedConnection);

    // CORREÇÃO: setAutoDelete(false) está ativo no RenderWorker para evitar
    // que a thread pool delete um QObject fora da UI thread (undefined behavior).
    // Em compensação, agendamos deleteLater() via QueuedConnection: após o
    // sinal chegar na UI thread, o QObject é destruído com segurança.
    // renderFailed também precisa de cleanup mesmo quando a página falha.
    connect(worker, &RenderWorker::pageRendered,
            worker, &QObject::deleteLater,
            Qt::QueuedConnection);
    connect(worker, &RenderWorker::renderFailed,
            worker, &QObject::deleteLater,
            Qt::QueuedConnection);

    m_pool->start(worker);
}

void PageCache::onPageRendered(int page, QImage image) {
    // Estamos de volta na UI thread — QPixmap::fromImage é seguro
    m_inFlight.remove(page);
    auto* px = new QPixmap(QPixmap::fromImage(std::move(image)));
    const int cost = (px->width() * px->height() * 4) / (1024 * 1024); // MB
    m_cache.insert(page, px, cost);
    emit pageReady(page);   // ReaderView conecta neste sinal para repintar
}

bool PageCache::isInFlight(int page) const {
    return m_inFlight.contains(page);
}
