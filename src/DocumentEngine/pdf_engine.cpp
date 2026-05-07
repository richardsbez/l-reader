// pdf_engine.cpp
#include "pdf_engine.hpp"

#include <QMutexLocker>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
PDFEngine::PDFEngine(QObject* parent)
    : DocumentEngine(parent)
{}

// ─────────────────────────────────────────────────────────────────────────────
// load
// ─────────────────────────────────────────────────────────────────────────────
bool PDFEngine::load(const QString& path)
{
    m_doc = Poppler::Document::load(path);

    if (!m_doc || m_doc->isLocked()) {
        const QString reason = !m_doc
            ? QStringLiteral("Não foi possível abrir o arquivo: %1").arg(path)
            : QStringLiteral("O PDF está protegido por senha: %1").arg(path);

        m_doc.reset();
        emit loadFailed(reason);
        return false;
    }

    m_doc->setRenderHint(Poppler::Document::Antialiasing,       true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing,   true);
    m_doc->setRenderHint(Poppler::Document::TextHinting,        true);

    emit documentLoaded(path, m_doc->numPages());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// pageCount / pageSize
// ─────────────────────────────────────────────────────────────────────────────
int PDFEngine::pageCount() const
{
    return m_doc ? m_doc->numPages() : 0;
}

QSizeF PDFEngine::pageSize(int page) const
{
    if (!m_doc) return {};
    const std::unique_ptr<Poppler::Page> p(m_doc->page(page));
    return p ? p->pageSizeF() : QSizeF{};
}

// ─────────────────────────────────────────────────────────────────────────────
// tocEntries — extrai o outline (bookmarks) do PDF via Poppler
//
// Poppler::Document::outline() percorre a tabela de conteúdos embutida no PDF
// (objeto /Outlines). É uma operação de leitura pura; segura para execução em
// thread de background enquanto o RenderWorker usa Page objects separados.
//
// Cada OutlineItem::destination() retorna uma LinkDestination com pageNumber()
// 1-based; convertemos para 0-based para consistência com PdfCanvasView.
// ─────────────────────────────────────────────────────────────────────────────
QList<TocEntry> PDFEngine::tocEntries() const
{
    if (!m_doc) return {};

    QList<TocEntry> result;
    result.reserve(64);   // a maioria dos livros tem menos de 64 capítulos
    flattenOutline(m_doc->outline(), result, 0);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// flattenOutline — converte a árvore recursiva do Poppler em lista plana
// ─────────────────────────────────────────────────────────────────────────────
void PDFEngine::flattenOutline(const QList<Poppler::OutlineItem>& items,
                               QList<TocEntry>& out, int depth)
{
    for (const Poppler::OutlineItem& item : items) {
        if (item.isNull()) continue;

        TocEntry entry;
        entry.title = item.name();
        entry.depth = depth;

        // destination() pode ser nulo para itens que agrupam sub-seções
        // sem destino de página próprio.
        const auto dest = item.destination();
        if (dest && dest->pageNumber() > 0)
            entry.page = dest->pageNumber() - 1;   // Poppler é 1-based

        out.append(entry);

        if (item.hasChildren())
            flattenOutline(item.children(), out, depth + 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// API de anotações (stubs)
// ─────────────────────────────────────────────────────────────────────────────
void PDFEngine::addHighlight(int page, const QRectF& rect, const QColor& color)
{
    QMutexLocker lock(&m_annotMutex);
    Q_UNUSED(page) Q_UNUSED(rect) Q_UNUSED(color)
}

void PDFEngine::addComment(int page, const QPointF& pos, const QString& text)
{
    QMutexLocker lock(&m_annotMutex);
    Q_UNUSED(page) Q_UNUSED(pos) Q_UNUSED(text)
}

bool PDFEngine::saveAnnotations()
{
    QMutexLocker lock(&m_annotMutex);
    if (!m_doc) return false;
    return true;
}
