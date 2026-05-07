// document_engine.hpp
#pragma once
#include <QList>
#include <QObject>
#include <QSizeF>
#include <QString>
#include <QUrl>
#include <memory>

enum class DocumentType { PDF, EPUB, MOBI, Markdown };

// ─── Entrada de Sumário — compartilhada por PDF e EPUB ────────────────────────
// • Para PDF  : page >= 0, url inválida.
// • Para EPUB : url válida, page == -1.
// depth 0 = item de raiz; incrementa a cada nível de hierarquia.
struct TocEntry {
    QString title;
    QUrl    url;
    int     page  = -1;   // índice 0-based (PDF)
    int     depth = 0;
};

// ─────────────────────────────────────────────────────────────────────────────

class DocumentEngine : public QObject {
    Q_OBJECT
public:
    explicit DocumentEngine(QObject* parent = nullptr) : QObject(parent) {}
    ~DocumentEngine() override = default;

    [[nodiscard]] virtual bool         load(const QString& path) = 0;
    [[nodiscard]] virtual int          pageCount()              const = 0;
    [[nodiscard]] virtual QSizeF       pageSize(int page)       const = 0;
    [[nodiscard]] virtual DocumentType type()                   const = 0;

    // Retorna o sumário do documento.
    // Chamável de qualquer thread após load() concluir.
    // Implementação padrão retorna lista vazia (engines sem TOC).
    [[nodiscard]] virtual QList<TocEntry> tocEntries() const { return {}; }

    // Factory — desacopla completamente a criação do tipo concreto
    static std::unique_ptr<DocumentEngine> create(const QString& path,
                                                   QObject* parent = nullptr);
signals:
    void documentLoaded(const QString& path, int pageCount);
    void loadFailed(const QString& error);
    void annotationChanged(int page);   // emitido pelo PDFAnnotWriter via bridge
};
