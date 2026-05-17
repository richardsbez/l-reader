// highlight_manager.hpp
// ─────────────────────────────────────────────────────────────────────────────
// HighlightManager — marca-textos com armazenamento dual-track.
//
// Estratégia de armazenamento
// ────────────────────────────
//  1. PRIMÁRIO — JSON sidecar (.highlights.json no notesDir)
//     • Leitura/escrita instantânea
//     • Não modifica o PDF; nunca corrompe o arquivo original
//     • Fonte de verdade enquanto o documento está aberto
//     • Funciona para PDF, EPUB e Markdown
//
//  2. SECUNDÁRIO — Anotação Highlight embutida no PDF (portátil)
//     • Acionada explicitamente por saveToPdf()
//     • Visível em Adobe Acrobat, Okular, Evince, Preview
//     • Usa Poppler::PDFConverter com WithChanges (incremental update)
//     • Disponível apenas para documentos PDF
//
// Carregamento
// ─────────────
//  loadFromJson()  → se encontrar o .json, usa-o como fonte de verdade
//  loadFromPdf()   → fallback: varre anotações Highlight do Poppler
//
// Thread-safety
// ─────────────
//  Todos os métodos devem ser chamados na thread principal.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "highlight_entry.hpp"

#include <poppler-qt6.h>
#include <QObject>
#include <QString>
#include <QVector>

class HighlightManager final : public QObject {
    Q_OBJECT
public:
    explicit HighlightManager(QObject* parent = nullptr);

    // ── Ciclo de vida ──────────────────────────────────────────────────────
    // doc     : ponteiro Poppler (nullptr para EPUB/Markdown — só JSON é usado)
    // filePath: caminho absoluto do documento (base para o nome do sidecar)
    // notesDir: diretório onde o .highlights.json será gravado
    void setDocument(Poppler::Document* doc,
                     const QString& filePath,
                     const QString& notesDir);
    void clear();

    // ── API de highlights ─────────────────────────────────────────────────
    void addHighlight   (const HighlightEntry& h);
    void removeHighlight(const QString& id);

    [[nodiscard]] const QVector<HighlightEntry>& highlights() const { return m_highlights; }
    [[nodiscard]] bool hasHighlight(const QString& id) const;

    // ── Persistência ──────────────────────────────────────────────────────
    // save()      → grava o JSON sidecar (rápido, sempre seguro)
    bool save();

    // saveToPdf() → embute como anotações Highlight no PDF (lento; portátil)
    //              Retorna false silenciosamente se não houver documento PDF.
    bool saveToPdf();

    [[nodiscard]] bool isDirty()        const { return m_dirty; }
    [[nodiscard]] bool isPdfOutOfSync() const { return m_pdfOutOfSync; }

signals:
    void highlightsChanged(const QVector<HighlightEntry>& highlights);
    void saved    (bool ok, const QString& errorMsg);
    void pdfSaved (bool ok, const QString& errorMsg);

private:
    // ── Carregamento ──────────────────────────────────────────────────────
    bool loadFromJson();   ///< true se encontrou e carregou o .json
    void loadFromPdf();    ///< fallback: varre anotações Highlight do Poppler

    // ── Helpers PDF ───────────────────────────────────────────────────────
    void syncAllPdfAnnotations();
    void insertPdfAnnotation   (const HighlightEntry& h);
    void removeAllPdfAnnotations();

    [[nodiscard]] QString jsonPath() const;

    // ── Estado ────────────────────────────────────────────────────────────
    Poppler::Document*    m_doc           = nullptr;
    QString               m_filePath;
    QString               m_notesDir;
    QVector<HighlightEntry> m_highlights;
    bool                  m_dirty         = false;
    bool                  m_pdfOutOfSync  = false;

    static constexpr const char* kAuthorTag   = "lexis-reader-hl";
    static constexpr const char* kJsonVersion = "1";
};
