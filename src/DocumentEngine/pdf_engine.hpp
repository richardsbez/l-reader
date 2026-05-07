// pdf_engine.hpp
#pragma once
#include "document_engine.hpp"
#include <poppler-qt6.h>
#include <QMutex>

class PDFEngine final : public DocumentEngine {
    Q_OBJECT
public:
    explicit PDFEngine(QObject* parent = nullptr);

    bool      load(const QString& path)  override;
    int       pageCount()                const override;
    QSizeF    pageSize(int page)         const override;
    DocumentType type()                  const override { return DocumentType::PDF; }

    // ── Sumário — extrai os bookmarks/outline do PDF via Poppler ────────────
    // Thread-safe para leitura: outline() não cria Page objects; lê somente
    // a tabela de conteúdos já carregada no PDFDoc interno do Poppler.
    [[nodiscard]] QList<TocEntry> tocEntries() const override;

    // Acesso thread-safe à página para o RenderWorker.
    [[nodiscard]] Poppler::Document* rawDocument() const { return m_doc.get(); }

    // API de anotações — delega ao PDFAnnotWriter
    void addHighlight(int page, const QRectF& rect, const QColor& color);
    void addComment  (int page, const QPointF& pos, const QString& text);
    bool saveAnnotations();

private:
    std::unique_ptr<Poppler::Document> m_doc;
    mutable QMutex m_annotMutex;

    // Helper recursivo: achata o outline hierárquico do Poppler em lista plana.
    static void flattenOutline(const QList<Poppler::OutlineItem>& items,
                               QList<TocEntry>& out, int depth);
};
