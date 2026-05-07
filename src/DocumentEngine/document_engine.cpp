
// document_engine.cpp
#include "document_engine.hpp"
#include "pdf_engine.hpp"
#include "ebook_engine.hpp"
#include "markdown_engine.hpp"

#include <QMimeDatabase>
#include <QMimeType>
#include <QFileInfo>

// ─────────────────────────────────────────────────────────────────────────────
// Factory — detecta o tipo pelo MIME e instancia o engine concreto.
//
// Ordem de detecção:
//   1. QMimeDatabase (conteúdo real do arquivo, não só a extensão)
//   2. Fallback por extensão — cobre casos onde o arquivo ainda não existe
//      em disco (ex: URL remota) ou tem extensão incomum.
//
// Retorna nullptr se o formato não for suportado; o chamador deve checar.
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<DocumentEngine>
DocumentEngine::create(const QString& path, QObject* parent)
{
    const QMimeDatabase db;
    const QMimeType     mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    const QString       name = mime.name();

    // ── PDF ──────────────────────────────────────────────────────────────────
    if (name == QLatin1String("application/pdf")) {
        return std::make_unique<PDFEngine>(parent);
    }

    // ── EPUB ─────────────────────────────────────────────────────────────────
    if (name == QLatin1String("application/epub+zip")) {
        return std::make_unique<EBookEngine>(parent);
    }

    // ── MOBI / AZW / AZW3 ────────────────────────────────────────────────────
    // O QMimeDatabase raramente tem entradas para MOBI; usamos a extensão
    // como fallback confiável.
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == QLatin1String("mobi") ||
            ext == QLatin1String("azw")  ||
            ext == QLatin1String("azw3"))
        {
            return std::make_unique<EBookEngine>(parent);
        }
    }

    // ── Markdown ─────────────────────────────────────────────────────────────
    if (name == QLatin1String("text/markdown")          ||
        name == QLatin1String("text/x-markdown")        ||
        name == QLatin1String("text/plain"))
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        // text/plain é genérico demais; só aceita se a extensão confirmar
        if (name != QLatin1String("text/plain") ||
            ext == QLatin1String("md")  ||
            ext == QLatin1String("markdown"))
        {
            return std::make_unique<MarkdownEngine>(parent);
        }
    }

    // Formato não suportado — retorna nullptr.
    // O chamador (MainWindow::openDocument) deve tratar este caso.
    return nullptr;
}
