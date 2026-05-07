// pdf_bookmark_manager.hpp
// ─────────────────────────────────────────────────────────────────────────────
// PdfBookmarkManager — marcadores com armazenamento dual-track.
//
// Estratégia de armazenamento
// ────────────────────────────
//  1. PRIMÁRIO — JSON sidecar (.bookmarks.json no notesDir)
//     • Leitura/escrita instantânea
//     • Não modifica o PDF; nunca corrompe o arquivo original
//     • Fonte de verdade enquanto o documento está aberto
//
//  2. SECUNDÁRIO — Anotação /Text embutida no PDF (portátil)
//     • Acionada explicitamente por saveToPdf()
//     • Visível em Adobe Acrobat, Okular, Evince, Preview
//     • Usa Poppler::PDFConverter com WithChanges (incremental update)
//
// Carregamento
// ─────────────
//  loadFromJson()  → se encontrar o .json, usa-o como fonte
//  loadFromPdf()   → fallback: varre anotações /Text do Poppler
//
// Thread-safety
// ─────────────
//  Todos os métodos devem ser chamados na thread principal.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <poppler-qt6.h>
#include <QList>
#include <QObject>
#include <QString>

// ─── Dados de um marcador ─────────────────────────────────────────────────────
struct BookmarkEntry {
    int     page    = -1;   // índice 0-based
    QString label;           // rótulo editável pelo usuário
    QString annotId;         // uniqueName() da anotação Poppler (se embutido)

    bool operator<(const BookmarkEntry& o) const { return page < o.page; }
};

// ─────────────────────────────────────────────────────────────────────────────

class PdfBookmarkManager final : public QObject {
    Q_OBJECT
public:
    explicit PdfBookmarkManager(QObject* parent = nullptr);

    // ── Ciclo de vida ──────────────────────────────────────────────────────
    // notesDir: diretório onde o sidecar JSON será gravado.
    void setDocument(Poppler::Document* doc,
                     const QString& filePath,
                     const QString& notesDir);
    void clear();

    // ── API de marcadores ─────────────────────────────────────────────────
    void addBookmark   (int page, const QString& label = {});
    void removeBookmark(int page);
    void toggleBookmark(int page, const QString& label = {});
    void renameBookmark(int page, const QString& newLabel);

    [[nodiscard]] bool hasBookmark(int page) const;
    [[nodiscard]] const QList<BookmarkEntry>& bookmarks() const { return m_bookmarks; }

    // ── Persistência ──────────────────────────────────────────────────────
    // save() → grava o JSON sidecar (rápido, sempre seguro).
    bool save();

    // saveToPdf() → embute no PDF via Poppler (lento; para portabilidade).
    // Deve ser chamado explicitamente pelo usuário ou no fechamento.
    bool saveToPdf();

    [[nodiscard]] bool isDirty()      const { return m_dirty; }
    [[nodiscard]] bool isPdfOutOfSync() const { return m_pdfOutOfSync; }

signals:
    void bookmarksChanged(const QList<BookmarkEntry>& bookmarks);
    // ok=true em sucesso; ok=false com mensagem de erro
    void saved(bool ok, const QString& errorMsg);
    void pdfSaved(bool ok, const QString& errorMsg);

private:
    // ── Carregamento ──────────────────────────────────────────────────────
    bool loadFromJson();   // retorna true se encontrou o .json
    void loadFromPdf();    // fallback: varre anotações do Poppler

    // ── Helpers internos ──────────────────────────────────────────────────
    int  findIndex(int page) const;

    // Insere/remove anotação Poppler na página (para saveToPdf)
    QString insertPdfAnnotation(int page, const QString& label);
    void    deletePdfAnnotation(int page, const QString& annotId);

    // Sincroniza todas as anotações PDF com m_bookmarks
    void syncAllPdfAnnotations();

    [[nodiscard]] QString jsonPath()  const;

    // ── Estado ────────────────────────────────────────────────────────────
    Poppler::Document*   m_doc          = nullptr;
    QString              m_filePath;
    QString              m_notesDir;
    QList<BookmarkEntry> m_bookmarks;
    bool                 m_dirty        = false;  // JSON desatualizado
    bool                 m_pdfOutOfSync = false;  // PDF desatualizado

    static constexpr const char* kAuthorTag    = "lexis-reader";
    static constexpr const char* kJsonVersion  = "1";
};
