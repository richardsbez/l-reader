// page_cache.hpp
#pragma once
#include <QObject>
#include <QCache>
#include <QPixmap>
#include <QThreadPool>
#include <optional>
#include "render_worker.hpp"

class PageCache final : public QObject {
    Q_OBJECT
public:
    static constexpr int PRE_BACK    = 2;    // N-2
    static constexpr int PRE_FORWARD = 5;    // N+5
    static constexpr int MAX_COST    = 100;  // MB — QCache usa "custo" por item

    explicit PageCache(QObject* parent = nullptr);

    void setEngine(Poppler::Document* doc, int pageCount);

    [[nodiscard]] std::optional<QPixmap> get(int page) const;

    // Chamado pelo PdfCanvasView ao mudar de página
    void onCurrentPageChanged(int page);

signals:
    // Emitido na UI thread quando uma página termina de renderizar.
    // PdfCanvasView conecta neste sinal para chamar update().
    void pageReady(int page);

private slots:
    // Slot que recebe o sinal do RenderWorker — SEMPRE roda na UI thread
    // graças ao Qt::QueuedConnection configurado em scheduleRender().
    void onPageRendered(int page, QImage image);

private:
    void scheduleRender(int page);
    bool isInFlight(int page) const;

    QCache<int, QPixmap>    m_cache;         // LRU built-in do Qt
    QSet<int>               m_inFlight;      // páginas sendo renderizadas
    QThreadPool*            m_pool;
    Poppler::Document*      m_doc   = nullptr;
    int                     m_count = 0;
    qreal                   m_dpi   = 150.0;
};
