// markdown_engine.cpp
#include "markdown_engine.hpp"

#include <QFile>
#include <QTextStream>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPageSize>
#include <QSizeF>

// ─────────────────────────────────────────────────────────────────────────────
// Tamanho de página padrão para Markdown: A4 em pontos (pt),
// com margens de 40pt em cada lado → área útil de 515 x 762 pt.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr qreal PAGE_W_PT  = 595.0;
static constexpr qreal PAGE_H_PT  = 842.0;
static constexpr qreal MARGIN_PT  = 40.0;
static constexpr qreal CONTENT_W  = PAGE_W_PT  - 2 * MARGIN_PT;
static constexpr qreal CONTENT_H  = PAGE_H_PT  - 2 * MARGIN_PT;

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
MarkdownEngine::MarkdownEngine(QObject* parent)
    : DocumentEngine(parent)
    , m_textDoc(std::make_unique<QTextDocument>())
{
    // Fonte padrão para renderização Markdown
    QFont font(QStringLiteral("serif"), 12);
    m_textDoc->setDefaultFont(font);

    // Largura do documento = área útil da página
    m_textDoc->setTextWidth(CONTENT_W);
}

// ─────────────────────────────────────────────────────────────────────────────
// load — lê o arquivo, interpreta como Markdown e calcula o número de páginas
// ─────────────────────────────────────────────────────────────────────────────
bool MarkdownEngine::load(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit loadFailed(
            QStringLiteral("Não foi possível abrir o arquivo Markdown: %1").arg(path));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString content = stream.readAll();

    if (content.isEmpty()) {
        emit loadFailed(
            QStringLiteral("O arquivo Markdown está vazio: %1").arg(path));
        return false;
    }

    // Qt6.4+ suporta Markdown nativamente via setMarkdown()
    m_textDoc->setMarkdown(content,
        QTextDocument::MarkdownDialectGitHub);

    // Força o layout para que o documentLayout() calcule a altura real
    m_textDoc->setPageSize(QSizeF(CONTENT_W, CONTENT_H));

    emit documentLoaded(path, pageCount());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// pageCount — calculado a partir da altura total do documento
// ─────────────────────────────────────────────────────────────────────────────
int MarkdownEngine::pageCount() const
{
    if (!m_textDoc) return 0;

    // QTextDocument com pageSize definido calcula o número de páginas
    // internamente. Usamos a altura total dividida pela altura de página.
    const qreal totalHeight = m_textDoc->size().height();
    const int   pages       = static_cast<int>(
                                  std::ceil(totalHeight / CONTENT_H));
    return std::max(1, pages);
}

// ─────────────────────────────────────────────────────────────────────────────
// pageSize — todas as páginas têm o mesmo tamanho (A4)
// ─────────────────────────────────────────────────────────────────────────────
QSizeF MarkdownEngine::pageSize(int /*page*/) const
{
    return QSizeF(PAGE_W_PT, PAGE_H_PT);
}
