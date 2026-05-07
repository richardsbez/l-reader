// render_worker.hpp
#pragma once
#include <QObject>
#include <QRunnable>
#include <QPixmap>
#include <poppler-qt6.h>

// Herda QObject (para emitir sinais) e QRunnable (para o pool).
// A thread pool chama run(); o sinal sobe para a UI thread via event loop.
class RenderWorker final : public QObject, public QRunnable {
    Q_OBJECT
public:
    RenderWorker(Poppler::Document* doc,  // non-owning, leitura apenas
                 int  page,
                 qreal dpi,
                 QObject* parent = nullptr)
        : QObject(parent), m_doc(doc), m_page(page), m_dpi(dpi)
    {
        setAutoDelete(false);  // gerenciamos lifetime manualmente via shared_ptr
    }

    void run() override {
        // Executado em worker thread — NUNCA toca em widgets aqui
        auto popplerPage = std::unique_ptr<Poppler::Page>(m_doc->page(m_page));
        if (!popplerPage) {
            emit renderFailed(m_page, QStringLiteral("page() retornou null"));
            return;
        }
        const QImage img = popplerPage->renderToImage(m_dpi, m_dpi);
        // QPixmap::fromImage dentro do sinal seria unsafe — convertemos no slot
        emit pageRendered(m_page, img);
    }

signals:
    void pageRendered(int page, QImage image);   // Q_ARG seguro entre threads
    void renderFailed(int page, QString error);

private:
    Poppler::Document* m_doc;
    int   m_page;
    qreal m_dpi;
};
