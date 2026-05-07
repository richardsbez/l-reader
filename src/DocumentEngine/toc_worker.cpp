// toc_worker.cpp
#include "toc_worker.hpp"
#include <QtConcurrent/QtConcurrentRun>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
TocWorker::TocWorker(QObject* parent)
    : QObject(parent)
{
    // finished() é emitido na thread principal via QFutureWatcher (queued).
    connect(&m_watcher, &QFutureWatcher<QList<TocEntry>>::finished,
            this, [this] {
        if (m_watcher.isCanceled()) return;

        auto entries = m_watcher.result();
        if (entries.isEmpty())
            emit tocEmpty();
        else
            emit tocReady(std::move(entries));
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// extract — lança a extração no thread pool global do Qt
// ─────────────────────────────────────────────────────────────────────────────
void TocWorker::extract(const DocumentEngine* engine)
{
    if (!engine) {
        emit tocEmpty();
        return;
    }

    // Cancela qualquer extração anterior antes de iniciar nova.
    m_watcher.cancel();
    m_watcher.waitForFinished();   // garante que a lambda anterior não acessa
                                   // um engine já destruído

    m_watcher.setFuture(
        QtConcurrent::run([engine]() -> QList<TocEntry> {
            return engine->tocEntries();
        })
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// cancel
// ─────────────────────────────────────────────────────────────────────────────
void TocWorker::cancel()
{
    m_watcher.cancel();
}
