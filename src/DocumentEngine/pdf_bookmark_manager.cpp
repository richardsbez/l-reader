// pdf_bookmark_manager.cpp
#include "pdf_bookmark_manager.hpp"

#include <poppler-annotation.h>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Posição normalizada da anotação: canto superior-direito da página
// ─────────────────────────────────────────────────────────────────────────────
static const QRectF kAnnotRect { 0.93, 0.01, 0.06, 0.05 };

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
PdfBookmarkManager::PdfBookmarkManager(QObject* parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument — inicializa o manager para um novo arquivo PDF
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::setDocument(Poppler::Document* doc,
                                      const QString& filePath,
                                      const QString& notesDir)
{
    m_doc          = doc;
    m_filePath     = filePath;
    m_notesDir     = notesDir;
    m_dirty        = false;
    m_pdfOutOfSync = false;
    m_bookmarks.clear();

    if (!m_doc) {
        // Documento nulo: notifica a UI para limpar a lista
        emit bookmarksChanged(m_bookmarks);
        return;
    }

    // Tenta carregar do JSON sidecar primeiro; fallback para anotações do PDF
    if (!loadFromJson())
        loadFromPdf();

    // Notifica a UI com os marcadores carregados (ou lista vazia)
    emit bookmarksChanged(m_bookmarks);
}

void PdfBookmarkManager::clear()
{
    m_doc          = nullptr;
    m_filePath.clear();
    m_notesDir.clear();
    m_bookmarks.clear();
    m_dirty        = false;
    m_pdfOutOfSync = false;
    // Notifica a UI para limpar o painel de marcadores
    emit bookmarksChanged(m_bookmarks);
}

// ─────────────────────────────────────────────────────────────────────────────
// jsonPath — caminho do sidecar JSON
//
// Exemplo: notesDir/Java_-_Como_Programar.pdf.bookmarks.json
// ─────────────────────────────────────────────────────────────────────────────
QString PdfBookmarkManager::jsonPath() const
{
    const QString baseName = QFileInfo(m_filePath).fileName()
                                 .replace(QLatin1Char('/'), QLatin1Char('_'))
                                 .replace(QLatin1Char(' '), QLatin1Char('_'));
    return m_notesDir + QLatin1Char('/') + baseName
           + QLatin1String(".bookmarks.json");
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromJson — carrega do sidecar JSON; retorna true se bem-sucedido
// ─────────────────────────────────────────────────────────────────────────────
bool PdfBookmarkManager::loadFromJson()
{
    const QString path = jsonPath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isNull() || !doc.isObject()) return false;

    const QJsonObject root = doc.object();
    // Verificação de versão — preparação para migrações futuras
    // const QString ver = root[QLatin1String("version")].toString();

    const QJsonArray arr = root[QLatin1String("bookmarks")].toArray();
    m_bookmarks.clear();
    m_bookmarks.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        const QJsonObject obj = v.toObject();
        BookmarkEntry entry;
        entry.page    = obj[QLatin1String("page")].toInt(-1);
        entry.label   = obj[QLatin1String("label")].toString();
        entry.annotId = obj[QLatin1String("annotId")].toString();
        if (entry.page >= 0)
            m_bookmarks.append(entry);
    }

    std::sort(m_bookmarks.begin(), m_bookmarks.end());
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromPdf — fallback: varre anotações /Text do Poppler
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::loadFromPdf()
{
    if (!m_doc) return;

    const int total = m_doc->numPages();
    for (int i = 0; i < total; ++i) {
        const std::unique_ptr<Poppler::Page> pg(m_doc->page(i));
        if (!pg) continue;

        for (const auto& ann : pg->annotations()) {
            if (!ann) continue;
            if (ann->author() != QString::fromLatin1(kAuthorTag)) continue;

            BookmarkEntry entry;
            entry.page    = i;
            entry.label   = ann->contents().isEmpty()
                                ? tr("Página %1").arg(i + 1)
                                : ann->contents();
            entry.annotId = ann->uniqueName();
            m_bookmarks.append(entry);
        }
    }

    std::sort(m_bookmarks.begin(), m_bookmarks.end());

    // Se carregou do PDF, persiste logo no JSON para próxima abertura
    if (!m_bookmarks.isEmpty()) {
        m_dirty = true;
        save();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// findIndex
// ─────────────────────────────────────────────────────────────────────────────
int PdfBookmarkManager::findIndex(int page) const
{
    for (int i = 0; i < m_bookmarks.size(); ++i)
        if (m_bookmarks[i].page == page) return i;
    return -1;
}

bool PdfBookmarkManager::hasBookmark(int page) const
{
    return findIndex(page) != -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// addBookmark
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::addBookmark(int page, const QString& label)
{
    if (!m_doc || page < 0 || page >= m_doc->numPages()) return;

    // Substitui se já existe
    const int existing = findIndex(page);
    if (existing != -1)
        m_bookmarks.removeAt(existing);

    BookmarkEntry entry;
    entry.page  = page;
    entry.label = label.isEmpty() ? tr("Página %1").arg(page + 1) : label;
    // annotId preenchido em saveToPdf(); JSON não precisa dele

    auto it = std::lower_bound(m_bookmarks.begin(), m_bookmarks.end(), entry);
    m_bookmarks.insert(it, entry);

    m_dirty        = true;
    m_pdfOutOfSync = true;
    emit bookmarksChanged(m_bookmarks);
}

// ─────────────────────────────────────────────────────────────────────────────
// removeBookmark
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::removeBookmark(int page)
{
    const int idx = findIndex(page);
    if (idx == -1) return;

    // Remove anotação do PDF se existir
    if (!m_bookmarks[idx].annotId.isEmpty())
        deletePdfAnnotation(page, m_bookmarks[idx].annotId);

    m_bookmarks.removeAt(idx);

    m_dirty        = true;
    m_pdfOutOfSync = true;
    emit bookmarksChanged(m_bookmarks);
}

// ─────────────────────────────────────────────────────────────────────────────
// toggleBookmark
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::toggleBookmark(int page, const QString& label)
{
    if (hasBookmark(page))
        removeBookmark(page);
    else
        addBookmark(page, label);
}

// ─────────────────────────────────────────────────────────────────────────────
// renameBookmark — edição inline do rótulo
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::renameBookmark(int page, const QString& newLabel)
{
    const int idx = findIndex(page);
    if (idx == -1) return;

    const QString trimmed = newLabel.trimmed();
    if (trimmed.isEmpty() || trimmed == m_bookmarks[idx].label) return;

    m_bookmarks[idx].label = trimmed;
    m_dirty        = true;
    m_pdfOutOfSync = true;
    emit bookmarksChanged(m_bookmarks);
}

// ─────────────────────────────────────────────────────────────────────────────
// save — persiste no JSON sidecar (rápido, não toca o PDF)
// ─────────────────────────────────────────────────────────────────────────────
bool PdfBookmarkManager::save()
{
    if (m_notesDir.isEmpty() || m_filePath.isEmpty()) {
        emit saved(false, tr("Diretório de notas não configurado."));
        return false;
    }

    if (!m_dirty) {
        emit saved(true, {});
        return true;
    }

    QDir().mkpath(m_notesDir);

    QJsonArray arr;
    for (const BookmarkEntry& bm : m_bookmarks) {
        QJsonObject obj;
        obj[QLatin1String("page")]    = bm.page;
        obj[QLatin1String("label")]   = bm.label;
        obj[QLatin1String("annotId")] = bm.annotId;
        arr.append(obj);
    }

    QJsonObject root;
    root[QLatin1String("version")]       = QString::fromLatin1(kJsonVersion);
    root[QLatin1String("file")]          = QFileInfo(m_filePath).fileName();
    root[QLatin1String("savedAt")]       = QDateTime::currentDateTime()
                                               .toString(Qt::ISODate);
    root[QLatin1String("bookmarks")]     = arr;

    const QString path    = jsonPath();
    const QString tmpPath = path + QLatin1String(".tmp");

    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit saved(false, tr("Não foi possível escrever: %1").arg(tmpPath));
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    QFile::remove(path);
    if (!QFile::rename(tmpPath, path)) {
        QFile::remove(tmpPath);
        emit saved(false, tr("Erro ao salvar sidecar JSON."));
        return false;
    }

    m_dirty = false;
    emit saved(true, {});
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// saveToPdf — embute marcadores como anotações /Text no PDF (portátil)
//
// Fluxo: 1) remove todas as nossas anotações antigas  2) insere as atuais
//         3) salva incrementalmente via PDFConverter::WithChanges
// ─────────────────────────────────────────────────────────────────────────────
bool PdfBookmarkManager::saveToPdf()
{
    if (!m_doc || m_filePath.isEmpty()) {
        emit pdfSaved(false, tr("Nenhum documento carregado."));
        return false;
    }

    syncAllPdfAnnotations();

    const QString tmpPath = m_filePath + QLatin1String(".lexis_tmp");
    QFile::remove(tmpPath);

    std::unique_ptr<Poppler::PDFConverter> conv(m_doc->pdfConverter());
    conv->setOutputFileName(tmpPath);
    conv->setPDFOptions(Poppler::PDFConverter::WithChanges);

    if (!conv->convert()) {
        QFile::remove(tmpPath);
        emit pdfSaved(false, tr("Falha ao escrever anotações no PDF."));
        return false;
    }

    if (!QFile::remove(m_filePath)) {
        QFile::remove(tmpPath);
        emit pdfSaved(false, tr("Sem permissão de escrita: %1").arg(m_filePath));
        return false;
    }

    if (!QFile::rename(tmpPath, m_filePath)) {
        QFile::rename(tmpPath, m_filePath);  // tenta restaurar
        emit pdfSaved(false, tr("Erro ao substituir arquivo PDF."));
        return false;
    }

    m_pdfOutOfSync = false;
    // Persiste os annotIds atualizados no JSON
    m_dirty = true;
    save();

    emit pdfSaved(true, {});
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// syncAllPdfAnnotations — remove as antigas e insere as atuais
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::syncAllPdfAnnotations()
{
    if (!m_doc) return;

    // 1) Remove todas as anotações lexis-reader existentes
    const int total = m_doc->numPages();
    for (int i = 0; i < total; ++i) {
        const std::unique_ptr<Poppler::Page> pg(m_doc->page(i));
        if (!pg) continue;
        for (const auto& ann : pg->annotations()) {
            if (ann && ann->author() == QString::fromLatin1(kAuthorTag))
                pg->removeAnnotation(ann.get());
        }
    }

    // 2) Insere as atuais e atualiza annotId
    for (BookmarkEntry& bm : m_bookmarks)
        bm.annotId = insertPdfAnnotation(bm.page, bm.label);
}

// ─────────────────────────────────────────────────────────────────────────────
// insertPdfAnnotation
// ─────────────────────────────────────────────────────────────────────────────
QString PdfBookmarkManager::insertPdfAnnotation(int page, const QString& label)
{
    const std::unique_ptr<Poppler::Page> pg(m_doc->page(page));
    if (!pg) return {};

    auto annot = std::make_unique<Poppler::TextAnnotation>(
        Poppler::TextAnnotation::Linked);

    Poppler::Annotation::Style style;
    style.setColor(QColor(0xFF, 0xD7, 0x00));
    style.setWidth(1.5);
    annot->setStyle(style);

    annot->setAuthor(QString::fromLatin1(kAuthorTag));
    annot->setContents(label);
    annot->setCreationDate(QDateTime::currentDateTime());
    annot->setModificationDate(QDateTime::currentDateTime());
    annot->setBoundary(kAnnotRect);

    pg->addAnnotation(annot.get());
    return annot->uniqueName();
}

// ─────────────────────────────────────────────────────────────────────────────
// deletePdfAnnotation
// ─────────────────────────────────────────────────────────────────────────────
void PdfBookmarkManager::deletePdfAnnotation(int page, const QString& annotId)
{
    if (!m_doc || annotId.isEmpty()) return;
    const std::unique_ptr<Poppler::Page> pg(m_doc->page(page));
    if (!pg) return;

    for (const auto& ann : pg->annotations()) {
        if (ann && ann->uniqueName() == annotId) {
            pg->removeAnnotation(ann.get());
            break;
        }
    }
}
