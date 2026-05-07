// toc_worker.hpp
// ─────────────────────────────────────────────────────────────────────────────
// TocWorker — extrai o sumário de qualquer DocumentEngine de forma assíncrona.
//
// Uso:
//   m_tocWorker->extract(m_engine.get());
//   connect(m_tocWorker, &TocWorker::tocReady, this, &MainWindow::onTocReady);
//
// Notas de thread-safety:
//   • PDFEngine::tocEntries() usa Poppler::Document::outline() — operação
//     somente-leitura que não conflita com a renderização de páginas em
//     RenderWorker (Poppler permite leituras concorrentes sobre o documento
//     enquanto os Page objects são criados em threads separadas).
//   • EBookEngine::tocEntries() retorna uma cópia de QList<TocEntry> já
//     populada durante o parse; é portanto trivialmente thread-safe.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "document_engine.hpp"

#include <QFutureWatcher>
#include <QObject>

class TocWorker final : public QObject {
    Q_OBJECT
public:
    explicit TocWorker(QObject* parent = nullptr);

    // Inicia (ou reinicia) a extração assíncrona.
    // engine deve sobreviver ao worker; cancelar() ou destruir o worker
    // antes de destruir o engine se necessário.
    void extract(const DocumentEngine* engine);

    // Cancela extração em andamento (sem-op se já concluída).
    void cancel();

    [[nodiscard]] bool isRunning() const { return m_watcher.isRunning(); }

signals:
    // Emitido na thread principal quando a extração termina com sucesso.
    void tocReady(QList<TocEntry> entries);
    // Emitido na thread principal se engine == nullptr ou a lista ficou vazia.
    void tocEmpty();

private:
    QFutureWatcher<QList<TocEntry>> m_watcher;
};
