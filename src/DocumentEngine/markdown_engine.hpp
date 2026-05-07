#pragma once
#include "document_engine.hpp"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

class MarkdownEngine final : public DocumentEngine {
    Q_OBJECT
public:
    explicit MarkdownEngine(QObject* parent = nullptr);

    bool    load(const QString& path)  override;
    int     pageCount()                const override;
    QSizeF  pageSize(int page)         const override;
    DocumentType type()                const override { return DocumentType::Markdown; }

    // Retorna o documento para que o RenderWorker possa desenhá-lo com QPainter
    QTextDocument* textDocument() const { return m_textDoc.get(); }

private:
    std::unique_ptr<QTextDocument> m_textDoc;
};
