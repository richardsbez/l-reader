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

    // Atualiza o DPI de renderização e invalida o cache inteiro.
    // Chamado em onZoomChanged() para que novas renders saiam nítidas.
    void setDpi(qreal dpi);

    [[nodiscard]] std::optional<QPixmap> get(int page) const;

    // Chamado pelo PdfCanvasView ao mudar de página
    void onCurrentPageChanged(int page);

    // Chamado pela navegação principal — usa pool de alta prioridade
    void requestRender(int page);

    // Chamado pela Galeria — usa pool de baixa prioridade separado,
    // evitando que thumbnails roubem threads do render principal.
    void requestGalleryRender(int page);

signals:
    // Emitido na UI thread quando uma página termina de renderizar.
    // PdfCanvasView conecta neste sinal para chamar update().
    void pageReady(int page);

private slots:
    // Slot que recebe o sinal do RenderWorker — SEMPRE roda na UI thread
    // graças ao Qt::QueuedConnection configurado em scheduleRender().
    // O parâmetro generation descarta resultados de gerações obsoletas
    // (ex: render em DPI antigo entregue após setDpi()).
    void onPageRendered(int page, QImage image, int generation);

private:
    void scheduleRender(int page, QThreadPool* pool);
    bool isInFlight(int page) const;

    QCache<int, QPixmap>    m_cache;         // LRU built-in do Qt
    QSet<int>               m_inFlight;      // páginas sendo renderizadas
    QThreadPool*            m_pool;          // pool principal (alta prioridade)
    QThreadPool*            m_galleryPool;   // pool da galeria (baixa prioridade)
    Poppler::Document*      m_doc        = nullptr;
    int                     m_count      = 0;
    qreal                   m_dpi        = 150.0;
    int                     m_generation = 0;  // incrementado em setDpi/setEngine
};
