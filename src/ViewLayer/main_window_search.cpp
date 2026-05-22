// main_window_search.cpp  —  l-reader
//
// Responsabilidades:
//   • Busca de texto em todas as páginas do PDF via Poppler
//   • Execução em thread de fundo (QtConcurrent::run) para não bloquear a UI
//   • Atualização da lista de resultados e navegação por clique

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"
#include "DocumentEngine/pdf_engine.hpp"

#include <QFutureWatcher>
#include <QtConcurrent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStringBuilder>
#include <poppler-qt6.h>

// ─────────────────────────────────────────────────────────────────────────────
// runSearch — pesquisa texto em todas as páginas do PDF via Poppler.
//
// Limita a 500 resultados para não sobrecarregar a lista.
// Executa em background; a UI é atualizada no slot do QFutureWatcher.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::runSearch()
{
    if (!m_searchInput || !m_searchResults || !m_searchStatus) return;

    const QString query = m_searchInput->text().trimmed();

    m_searchResults->clear();
    m_searchStatus->hide();

    if (m_pdfView) m_pdfView->clearSearchHighlights();

    if (query.isEmpty()) return;

    if (!m_engine || m_engine->type() != DocumentType::PDF) {
        m_searchStatus->setText(tr("Busca disponível apenas em documentos PDF."));
        m_searchStatus->show();
        return;
    }

    auto* pdf = qobject_cast<PDFEngine*>(m_engine.get());
    if (!pdf) return;

    Poppler::Document* doc = pdf->rawDocument();
    if (!doc) return;

    m_searchStatus->setText(tr("Buscando…"));
    m_searchStatus->show();
    m_searchInput->setEnabled(false);

    // ── Estrutura de resultado ─────────────────────────────────────────────
    // rects: todos os rects da ocorrência em coordenadas de pontos PDF.
    struct Hit {
        int           page;
        QString       snippet;
        QList<QRectF> rects;
    };

    const int total = doc->numPages();

    auto* watcher = new QFutureWatcher<QList<Hit>>(this);

    connect(watcher, &QFutureWatcher<QList<Hit>>::finished, this,
            [this, watcher, query]() {
        const auto hits = watcher->result();
        watcher->deleteLater();

        m_searchInput->setEnabled(true);
        m_searchResults->clear();

        if (hits.isEmpty()) {
            m_searchStatus->setText(
                tr("Nenhum resultado para \"%1\".").arg(query));
        } else {
            m_searchStatus->setText(
                tr("%n resultado(s) para \"%1\"", "", hits.size()).arg(query));
            for (const auto& h : hits) {
                const QString label =
                    QStringLiteral("p.%1  %2")
                        .arg(h.page + 1)
                        .arg(h.snippet);
                auto* item = new QListWidgetItem(label, m_searchResults);
                item->setData(Qt::UserRole,     h.page);
                item->setData(Qt::UserRole + 1,
                              QVariant::fromValue<QList<QRectF>>(h.rects));
                item->setToolTip(h.snippet);
                m_searchResults->addItem(item);
            }
        }
        m_searchStatus->show();
    });

    // ── Busca em background ────────────────────────────────────────────────
    // Poppler::Page::search é thread-safe por página.
    auto future = QtConcurrent::run([doc, query, total]() -> QList<Hit> {
        QList<Hit> results;
        results.reserve(64);

        for (int i = 0; i < total && results.size() < 500; ++i) {
            auto pg = std::unique_ptr<Poppler::Page>(doc->page(i));
            if (!pg) continue;

            const QList<QRectF> pageHits =
                pg->search(query, Poppler::Page::IgnoreCase);

            for (const QRectF& rect : pageHits) {
                QRectF ctx = rect.adjusted(-80, -2, 80, 2);
                QString snippet = pg->text(ctx).simplified();
                if (snippet.length() > 70)
                    snippet = snippet.left(67) % QStringLiteral("…");
                if (snippet.isEmpty())
                    snippet = query;

                results.append({i, snippet, {rect}});

                if (results.size() >= 500) break;
            }
        }
        return results;
    });

    watcher->setFuture(future);
}
