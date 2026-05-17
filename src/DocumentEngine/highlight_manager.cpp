// highlight_manager.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Aplica a filosofia do Okular:
//
//  1. "RG" do arquivo — o sidecar JSON é nomeado pelo hash do conteúdo
//     (via DocumentIdentity), não pelo nome do arquivo.  Mover ou renomear
//     o PDF não perde nenhum highlight.
//
//  2. Camada transparente — os highlights nunca são gravados no PDF até que
//     saveToPdf() seja chamado explicitamente.  O JSON é a fonte de verdade.
//
//  3. Coordenadas normalizadas — o JSON armazena retângulos em fração [0,1]
//     relativos à dimensão da página.  Isso garante que o highlight caia
//     exatamente sobre a palavra correta em qualquer zoom, DPI ou tamanho
//     de tela.  Internamente (ptRects) continuamos usando pontos PDF para
//     compatibilidade com o Poppler e o canvas de renderização.
//
//  Versão do JSON: "2" (normalizado).  Arquivos v1 (pontos absolutos) são
//  lidos no modo legado e re-salvos automaticamente no novo formato.
// ─────────────────────────────────────────────────────────────────────────────
#include "highlight_manager.hpp"
#include "document_identity.hpp"

#include <poppler-annotation.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

static constexpr const char* kCurrentJsonVersion = "2";   // coordenadas normalizadas

HighlightManager::HighlightManager(QObject* parent)
    : QObject(parent)
{}

void HighlightManager::setDocument(Poppler::Document* doc,
                                    const QString& filePath,
                                    const QString& notesDir)
{
    m_doc          = doc;
    m_filePath     = filePath;
    m_notesDir     = notesDir;
    m_dirty        = false;
    m_pdfOutOfSync = false;
    m_highlights.clear();

    if (m_filePath.isEmpty()) {
        emit highlightsChanged(m_highlights);
        return;
    }

    if (!loadFromJson() && m_doc)
        loadFromPdf();

    emit highlightsChanged(m_highlights);
}

void HighlightManager::clear()
{
    m_doc = nullptr;
    m_filePath.clear();
    m_notesDir.clear();
    m_highlights.clear();
    m_dirty        = false;
    m_pdfOutOfSync = false;
    emit highlightsChanged(m_highlights);
}

// ─────────────────────────────────────────────────────────────────────────────
// jsonPath — usa o hash do conteúdo como nome (conceito Okular "RG do arquivo")
//
// Exemplo:  notesDir/a3f2c1b8e4d09712.highlights.json
// Fallback: nome do arquivo se o hash falhar.
// ─────────────────────────────────────────────────────────────────────────────
QString HighlightManager::jsonPath() const
{
    const QString id = DocumentIdentity::idForFile(m_filePath);

    const QString key = id.isEmpty()
        ? QFileInfo(m_filePath).fileName()
              .replace(QLatin1Char('/'), QLatin1Char('_'))
              .replace(QLatin1Char(' '), QLatin1Char('_'))
        : id;

    return m_notesDir + QLatin1Char('/') + key
           + QLatin1String(".highlights.json");
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de normalização (conceito Okular "Mapa de Porcentagem")
// ─────────────────────────────────────────────────────────────────────────────
static QSizeF pageSizeFor(Poppler::Document* doc, int page)
{
    if (!doc) return {};
    const std::unique_ptr<Poppler::Page> pg(doc->page(page));
    return pg ? pg->pageSizeF() : QSizeF{};
}

// ptRect → normRect  [0,1]
static QRectF toNorm(const QRectF& r, const QSizeF& ps)
{
    if (ps.isEmpty()) return r;
    return { r.x()      / ps.width(),  r.y()      / ps.height(),
             r.width()  / ps.width(),  r.height() / ps.height() };
}

// normRect [0,1] → ptRect
static QRectF fromNorm(const QRectF& r, const QSizeF& ps)
{
    if (ps.isEmpty()) return r;
    return { r.x()      * ps.width(),  r.y()      * ps.height(),
             r.width()  * ps.width(),  r.height() * ps.height() };
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromJson
//   version "1" → coords em pontos PDF (legado, migra automaticamente)
//   version "2" → coords normalizadas [0,1]  ← atual
// ─────────────────────────────────────────────────────────────────────────────
bool HighlightManager::loadFromJson()
{
    const QString path = jsonPath();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument jdoc = QJsonDocument::fromJson(f.readAll());
    if (jdoc.isNull() || !jdoc.isObject()) return false;

    const QJsonObject root = jdoc.object();
    const QJsonArray  arr  = root[QLatin1String("highlights")].toArray();

    const bool isNormalized =
        (root[QLatin1String("version")].toString() == QLatin1String("2"));

    m_highlights.clear();
    m_highlights.reserve(arr.size());

    for (const QJsonValue& v : arr) {
        const QJsonObject obj = v.toObject();

        HighlightEntry h;
        h.id   = obj[QLatin1String("id")].toString();
        h.text = obj[QLatin1String("text")].toString();

        const QString colorStr = obj[QLatin1String("color")].toString();
        if (!colorStr.isEmpty())
            h.color = QColor(colorStr);

        const QJsonArray spansArr = obj[QLatin1String("spans")].toArray();
        for (const QJsonValue& sv : spansArr) {
            const QJsonObject so = sv.toObject();
            HighlightPageSpan span;
            span.page = so[QLatin1String("page")].toInt(-1);
            if (span.page < 0) continue;

            const QSizeF ps = isNormalized ? pageSizeFor(m_doc, span.page)
                                           : QSizeF{};

            const QJsonArray rectsArr = so[QLatin1String("rects")].toArray();
            span.ptRects.reserve(rectsArr.size());
            for (const QJsonValue& rv : rectsArr) {
                const QJsonArray r = rv.toArray();
                if (r.size() == 4) {
                    QRectF raw(r[0].toDouble(), r[1].toDouble(),
                               r[2].toDouble(), r[3].toDouble());
                    span.ptRects.append(isNormalized ? fromNorm(raw, ps) : raw);
                }
            }
            if (!span.ptRects.isEmpty())
                h.spans.append(span);
        }

        if (h.isValid())
            m_highlights.append(h);
    }

    // Migra formato legado v1 → v2 automaticamente
    if (!isNormalized && !m_highlights.isEmpty()) {
        m_dirty = true;
        save();
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromPdf — fallback: varre anotações Highlight do Poppler
// ─────────────────────────────────────────────────────────────────────────────
void HighlightManager::loadFromPdf()
{
    if (!m_doc) return;

    const int total = m_doc->numPages();
    for (int i = 0; i < total; ++i) {
        const std::unique_ptr<Poppler::Page> pg(m_doc->page(i));
        if (!pg) continue;

        for (const auto& ann : pg->annotations(
                 QSet<Poppler::Annotation::SubType>{
                     Poppler::Annotation::AHighlight})) {
            if (!ann) continue;
            if (ann->author() != QString::fromLatin1(kAuthorTag)) continue;

            auto* ha = static_cast<Poppler::HighlightAnnotation*>(ann.get());

            HighlightEntry h;
            h.id    = ann->uniqueName();
            h.text  = ann->contents();
            h.color = QColor(255, 220, 0, 110);

            HighlightPageSpan span;
            span.page = i;

            const QSizeF pageSize = pg->pageSizeF();
            for (const auto& quad : ha->highlightQuads()) {
                const qreal x  = quad.points[0].x() * pageSize.width();
                const qreal y  = quad.points[0].y() * pageSize.height();
                const qreal w  = (quad.points[1].x() - quad.points[0].x()) * pageSize.width();
                const qreal h2 = (quad.points[3].y() - quad.points[0].y()) * pageSize.height();
                span.ptRects.append(QRectF(x, y, w, std::abs(h2)));
            }

            if (!span.ptRects.isEmpty()) {
                h.spans.append(span);
                m_highlights.append(h);
            }
        }
    }

    if (!m_highlights.isEmpty()) {
        m_dirty = true;
        save();
    }
}

bool HighlightManager::hasHighlight(const QString& id) const
{
    return std::any_of(m_highlights.begin(), m_highlights.end(),
                       [&id](const HighlightEntry& h){ return h.id == id; });
}

void HighlightManager::addHighlight(const HighlightEntry& h)
{
    if (!h.isValid()) return;

    for (auto& existing : m_highlights) {
        if (existing.id == h.id) {
            existing = h;
            m_dirty        = true;
            m_pdfOutOfSync = true;
            emit highlightsChanged(m_highlights);
            return;
        }
    }

    m_highlights.append(h);
    m_dirty        = true;
    m_pdfOutOfSync = true;
    emit highlightsChanged(m_highlights);
}

void HighlightManager::removeHighlight(const QString& id)
{
    const int before = m_highlights.size();
    m_highlights.erase(
        std::remove_if(m_highlights.begin(), m_highlights.end(),
                       [&id](const HighlightEntry& h){ return h.id == id; }),
        m_highlights.end());

    if (m_highlights.size() != before) {
        m_dirty        = true;
        m_pdfOutOfSync = true;
        emit highlightsChanged(m_highlights);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// save — JSON sidecar com coordenadas normalizadas (v2)
//
// Gravação atômica: escreve .tmp → rename, nunca deixa arquivo corrompido.
// ─────────────────────────────────────────────────────────────────────────────
bool HighlightManager::save()
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
    for (const HighlightEntry& h : m_highlights) {
        QJsonObject obj;
        obj[QLatin1String("id")]    = h.id;
        obj[QLatin1String("text")]  = h.text;
        obj[QLatin1String("color")] = h.color.name(QColor::HexArgb);

        QJsonArray spansArr;
        for (const HighlightPageSpan& span : h.spans) {
            QJsonObject so;
            so[QLatin1String("page")] = span.page;

            const QSizeF ps = pageSizeFor(m_doc, span.page);

            QJsonArray rectsArr;
            for (const QRectF& r : span.ptRects) {
                // Conceito Okular "Mapa de Porcentagem": salva como fração [0,1]
                // invariante de zoom, DPI e tamanho de tela.
                const QRectF nr = toNorm(r, ps);
                QJsonArray ra;
                ra.append(nr.x());
                ra.append(nr.y());
                ra.append(nr.width());
                ra.append(nr.height());
                rectsArr.append(ra);
            }
            so[QLatin1String("rects")] = rectsArr;
            spansArr.append(so);
        }
        obj[QLatin1String("spans")] = spansArr;
        arr.append(obj);
    }

    QJsonObject root;
    root[QLatin1String("version")]    = QLatin1String(kCurrentJsonVersion);
    root[QLatin1String("file")]       = QFileInfo(m_filePath).fileName();
    root[QLatin1String("fileId")]     = DocumentIdentity::idForFile(m_filePath);
    root[QLatin1String("savedAt")]    = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QLatin1String("highlights")] = arr;

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
        emit saved(false, tr("Erro ao salvar highlights JSON."));
        return false;
    }

    m_dirty = false;
    emit saved(true, {});
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// saveToPdf — "A Fusão" (conceito Okular):
// só toca o PDF quando o usuário pede explicitamente.
// ─────────────────────────────────────────────────────────────────────────────
bool HighlightManager::saveToPdf()
{
    if (!m_doc || m_filePath.isEmpty()) {
        emit pdfSaved(false, tr("Nenhum documento PDF carregado."));
        return false;
    }

    syncAllPdfAnnotations();

    const QString tmpPath = m_filePath + QLatin1String(".lexis_hl_tmp");
    QFile::remove(tmpPath);

    std::unique_ptr<Poppler::PDFConverter> conv(m_doc->pdfConverter());
    conv->setOutputFileName(tmpPath);
    conv->setPDFOptions(Poppler::PDFConverter::WithChanges);

    if (!conv->convert()) {
        QFile::remove(tmpPath);
        emit pdfSaved(false, tr("Falha ao escrever highlights no PDF."));
        return false;
    }

    if (!QFile::remove(m_filePath)) {
        QFile::remove(tmpPath);
        emit pdfSaved(false, tr("Sem permissão de escrita: %1").arg(m_filePath));
        return false;
    }

    if (!QFile::rename(tmpPath, m_filePath)) {
        emit pdfSaved(false, tr("Erro ao substituir arquivo PDF."));
        return false;
    }

    // O conteúdo do PDF mudou → o hash antigo é inválido
    DocumentIdentity::invalidate(m_filePath);

    m_pdfOutOfSync = false;
    m_dirty = true;
    save();   // re-salva JSON com novo fileId

    emit pdfSaved(true, {});
    return true;
}

void HighlightManager::syncAllPdfAnnotations()
{
    removeAllPdfAnnotations();
    for (const HighlightEntry& h : m_highlights)
        insertPdfAnnotation(h);
}

void HighlightManager::removeAllPdfAnnotations()
{
    if (!m_doc) return;
    const int total = m_doc->numPages();
    for (int i = 0; i < total; ++i) {
        const std::unique_ptr<Poppler::Page> pg(m_doc->page(i));
        if (!pg) continue;
        for (const auto& ann : pg->annotations()) {
            if (ann && ann->author() == QString::fromLatin1(kAuthorTag))
                pg->removeAnnotation(ann.get());
        }
    }
}

void HighlightManager::insertPdfAnnotation(const HighlightEntry& h)
{
    if (!m_doc) return;
    for (const HighlightPageSpan& span : h.spans) {
        const std::unique_ptr<Poppler::Page> pg(m_doc->page(span.page));
        if (!pg) continue;

        const QSizeF pageSize = pg->pageSizeF();

        auto ann = std::make_unique<Poppler::HighlightAnnotation>();
        ann->setAuthor(QString::fromLatin1(kAuthorTag));
        ann->setContents(h.text);
        ann->setCreationDate(QDateTime::currentDateTime());
        ann->setModificationDate(QDateTime::currentDateTime());

        Poppler::Annotation::Style style;
        style.setColor(h.color);
        style.setOpacity(h.color.alphaF());
        ann->setStyle(style);

        QList<Poppler::HighlightAnnotation::Quad> quads;
        for (const QRectF& r : span.ptRects) {
            Poppler::HighlightAnnotation::Quad q;
            q.points[0] = QPointF(r.left()  / pageSize.width(),  r.top()    / pageSize.height());
            q.points[1] = QPointF(r.right() / pageSize.width(),  r.top()    / pageSize.height());
            q.points[2] = QPointF(r.right() / pageSize.width(),  r.bottom() / pageSize.height());
            q.points[3] = QPointF(r.left()  / pageSize.width(),  r.bottom() / pageSize.height());
            q.capStart = q.capEnd = false;
            q.feather = 0.1;
            quads.append(q);
        }
        ann->setHighlightQuads(quads);

        if (!span.ptRects.isEmpty()) {
            QRectF bbox = span.ptRects.first();
            for (const QRectF& r : span.ptRects) bbox = bbox.united(r);
            ann->setBoundary(QRectF(
                bbox.x()      / pageSize.width(),
                bbox.y()      / pageSize.height(),
                bbox.width()  / pageSize.width(),
                bbox.height() / pageSize.height()));
        }

        pg->addAnnotation(ann.get());
    }
}
