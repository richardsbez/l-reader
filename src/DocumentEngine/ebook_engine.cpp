// ebook_engine.cpp
#include "ebook_engine.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDebug>

EBookEngine::EBookEngine(QObject* parent) : DocumentEngine(parent) {}

// ─── load ─────────────────────────────────────────────────────────────────────
bool EBookEngine::load(const QString& path)
{
    const QFileInfo info(path);
    const QString   ext = info.suffix().toLower();

    // MOBI: precisa do Calibre
    if (ext == QLatin1String("mobi") ||
        ext == QLatin1String("azw")  ||
        ext == QLatin1String("azw3"))
    {
        m_type = DocumentType::MOBI;

        QProcess check;
        check.start(QStringLiteral("ebook-convert"), {QStringLiteral("--version")});
        if (!check.waitForFinished(3000) || check.exitCode() != 0) {
            emit loadFailed(
                QStringLiteral("Suporte a MOBI/AZW requer o Calibre instalado.\n"
                               "Instale em https://calibre-ebook.com\nArquivo: %1").arg(path));
            return false;
        }
        const QString cacheRoot = QStandardPaths::writableLocation(
                                      QStandardPaths::AppDataLocation)
                                  + QStringLiteral("/epub_cache/");
        QDir().mkpath(cacheRoot);
        const QString outEpub = cacheRoot + info.completeBaseName()
                                + QStringLiteral("_converted.epub");
        QProcess conv;
        conv.start(QStringLiteral("ebook-convert"),
                   {path, outEpub, QStringLiteral("--no-default-epub-cover")});
        if (!conv.waitForFinished(60000) || conv.exitCode() != 0) {
            emit loadFailed(QStringLiteral("Falha na conversao MOBI->EPUB (cod %1).")
                            .arg(conv.exitCode()));
            return false;
        }
        return load(outEpub);
    }

    m_type = DocumentType::EPUB;
    m_spine.clear();
    m_toc.clear();
    m_title.clear();
    m_author.clear();

    const QString cacheRoot = QStandardPaths::writableLocation(
                                  QStandardPaths::AppDataLocation)
                              + QStringLiteral("/epub_cache/")
                              + info.completeBaseName();

    if (!extractZip(path, cacheRoot)) {
        emit loadFailed(QStringLiteral("Falha ao extrair EPUB: %1").arg(path));
        return false;
    }

    QString opfPath;
    if (!parseContainer(cacheRoot, opfPath)) {
        emit loadFailed(QStringLiteral("container.xml invalido: %1").arg(path));
        return false;
    }

    if (!parseOPF(opfPath)) {
        emit loadFailed(QStringLiteral("OPF invalido ou spine vazia: %1").arg(path));
        return false;
    }
    return true;
}

// ─── extractZip ───────────────────────────────────────────────────────────────
bool EBookEngine::extractZip(const QString& zipPath, const QString& destDir)
{
    QDir().mkpath(destDir);
    QProcess proc;
    proc.start(QStringLiteral("unzip"),
               {QStringLiteral("-o"), QStringLiteral("-q"),
                zipPath, QStringLiteral("-d"), destDir});
    return proc.waitForFinished(30000) && proc.exitCode() == 0;
}

// ─── parseContainer ───────────────────────────────────────────────────────────
bool EBookEngine::parseContainer(const QString& extractDir, QString& outOpfPath)
{
    const QString containerPath =
        extractDir + QLatin1String("/META-INF/container.xml");

    QFile f(containerPath);
    if (f.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(&f);
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() &&
                xml.name().toString().compare(QLatin1String("rootfile"),
                                              Qt::CaseInsensitive) == 0)
            {
                const QString rel = xml.attributes()
                                       .value(QLatin1String("full-path"))
                                       .toString();
                if (!rel.isEmpty()) {
                    outOpfPath = extractDir + QLatin1Char('/') + rel;
                    return true;
                }
            }
        }
    }

    // Fallback: qualquer .opf
    QDirIterator it(extractDir, {QStringLiteral("*.opf")},
                    QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) { outOpfPath = it.next(); return true; }
    return false;
}

// ─── parseOPF ─────────────────────────────────────────────────────────────────
bool EBookEngine::parseOPF(const QString& opfPath)
{
    QFile f(opfPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray raw = f.readAll();
    f.close();

    const QDir opfDir = QFileInfo(opfPath).absoluteDir();

    QHash<QString, QString> manifest;   // id -> href
    QHash<QString, QString> mediaTypes; // id -> media-type
    QString ncxId, navId;
    QStringList spineIdrefs;

    {
        QXmlStreamReader xml(raw);
        QString section;

        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            const QString tag = xml.name().toString().toLower();

            if (xml.isStartElement()) {
                if      (tag == QLatin1String("metadata"))  section = QStringLiteral("metadata");
                else if (tag == QLatin1String("manifest"))  section = QStringLiteral("manifest");
                else if (tag == QLatin1String("spine"))     section = QStringLiteral("spine");

                if (section == QLatin1String("metadata")) {
                    if (tag == QLatin1String("dc:title")   || tag == QLatin1String("title"))
                        m_title  = xml.readElementText().trimmed();
                    else if (tag == QLatin1String("dc:creator") || tag == QLatin1String("creator"))
                        m_author = xml.readElementText().trimmed();
                }

                if (section == QLatin1String("manifest") && tag == QLatin1String("item")) {
                    const auto attrs = xml.attributes();
                    const QString id    = attrs.value(QLatin1String("id")).toString();
                    const QString href  = attrs.value(QLatin1String("href")).toString();
                    const QString mt    = attrs.value(QLatin1String("media-type")).toString();
                    const QString props = attrs.value(QLatin1String("properties")).toString();
                    if (!id.isEmpty() && !href.isEmpty()) {
                        manifest[id]   = href;
                        mediaTypes[id] = mt;
                    }
                    if (props.contains(QLatin1String("nav")))
                        navId = id;
                    if (mt == QLatin1String("application/x-dtbncx+xml"))
                        ncxId = id;
                }

                if (section == QLatin1String("spine") && tag == QLatin1String("itemref")) {
                    const auto attrs  = xml.attributes();
                    const QString ref = attrs.value(QLatin1String("idref")).toString();
                    const QString lin = attrs.value(QLatin1String("linear")).toString().toLower();
                    if (!ref.isEmpty() && lin != QLatin1String("no"))
                        spineIdrefs << ref;
                }
            }

            if (xml.isEndElement()) {
                if (tag == QLatin1String("metadata") ||
                    tag == QLatin1String("manifest")  ||
                    tag == QLatin1String("spine"))
                    section.clear();
            }
        }
    }

    // Montar QList<QUrl> da spine
    for (const QString& idref : std::as_const(spineIdrefs)) {
        const QString href = manifest.value(idref);
        if (href.isEmpty()) continue;
        const QString hrefBase = href.section(QLatin1Char('#'), 0, 0);
        const QString abs = opfDir.absoluteFilePath(
            QUrl::fromPercentEncoding(hrefBase.toUtf8()));
        m_spine.append(QUrl::fromLocalFile(abs));
    }

    // Fallback agressivo se a spine ficou vazia
    if (m_spine.isEmpty()) {
        QDirIterator it(opfDir.absolutePath(),
                        {QStringLiteral("*.xhtml"), QStringLiteral("*.html")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            m_spine.append(QUrl::fromLocalFile(it.next()));
        if (m_spine.isEmpty()) return false;
    }

    if (m_title.isEmpty())
        m_title = QFileInfo(opfPath).completeBaseName();

    // TOC
    if (!navId.isEmpty() && manifest.contains(navId))
        parseNAV(opfDir.absoluteFilePath(manifest[navId]), opfDir);
    else if (!ncxId.isEmpty() && manifest.contains(ncxId))
        parseNCX(opfDir.absoluteFilePath(manifest[ncxId]), opfDir);

    emit documentLoaded(opfPath, m_spine.size());
    return true;
}

// ─── parseNCX — EPUB2 ─────────────────────────────────────────────────────────
void EBookEngine::parseNCX(const QString& ncxPath, const QDir& opfDir)
{
    QFile f(ncxPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QXmlStreamReader xml(&f);

    int     depth = 0;
    QString pendingUrl;
    QString pendingLabel;

    while (!xml.atEnd()) {
        xml.readNext();
        const QString tag = xml.name().toString().toLower();

        if (xml.isStartElement()) {
            if (tag == QLatin1String("navpoint")) {
                ++depth;
                pendingUrl.clear();
                pendingLabel.clear();
            } else if (tag == QLatin1String("content")) {
                const QString src = xml.attributes()
                                       .value(QLatin1String("src")).toString();
                pendingUrl = opfDir.absoluteFilePath(
                    QUrl::fromPercentEncoding(
                        src.section(QLatin1Char('#'), 0, 0).toUtf8()));
            } else if (tag == QLatin1String("text")) {
                pendingLabel = xml.readElementText().trimmed();
            }
        }

        if (xml.isEndElement() && tag == QLatin1String("navpoint")) {
            if (!pendingUrl.isEmpty() && !pendingLabel.isEmpty())
                m_toc.append({pendingLabel,
                              QUrl::fromLocalFile(pendingUrl),
                              depth - 1});
            --depth;
        }
    }
}

// ─── parseNAV — EPUB3 ─────────────────────────────────────────────────────────
void EBookEngine::parseNAV(const QString& navPath, const QDir& /*opfDir*/)
{
    QFile f(navPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QString content = QString::fromUtf8(f.readAll());

    // Extrai links do bloco epub:type="toc"; fallback: documento inteiro
    const int tocStart = content.indexOf(
        QRegularExpression(QStringLiteral(R"re(epub:type\s*=\s*"toc")re"),
                           QRegularExpression::CaseInsensitiveOption));
    const QString block = (tocStart >= 0) ? content.mid(tocStart) : content;

    const QDir navDir = QFileInfo(navPath).absoluteDir();

    static const QRegularExpression reA(
        QStringLiteral(R"re(<a[^>]+href\s*=\s*"([^"#][^"]*)"[^>]*>(.*?)</a>)re"),
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression reOlO(QStringLiteral("<ol[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reOlC(QStringLiteral("</ol>"),
        QRegularExpression::CaseInsensitiveOption);

    // pos usa qsizetype para evitar narrowing de capturedStart()
    struct Ev { qsizetype pos; int type; QString h, l; };
    QVector<Ev> evs;

    for (auto it = reOlO.globalMatch(block); it.hasNext(); ) {
        auto m = it.next();
        evs.append({m.capturedStart(), 0, {}, {}});
    }
    for (auto it = reOlC.globalMatch(block); it.hasNext(); ) {
        auto m = it.next();
        evs.append({m.capturedStart(), 1, {}, {}});
    }
    for (auto it = reA.globalMatch(block); it.hasNext(); ) {
        auto m = it.next();
        evs.append({m.capturedStart(), 2, m.captured(1), m.captured(2)});
    }
    std::sort(evs.begin(), evs.end(),
              [](const Ev& a, const Ev& b) { return a.pos < b.pos; });

    int depth = 0;
    for (const auto& ev : std::as_const(evs)) {
        if      (ev.type == 0) ++depth;
        else if (ev.type == 1) depth = std::max(0, depth - 1);
        else {
            const QString label = QString(ev.l)
                .remove(QRegularExpression(QStringLiteral("<[^>]*>"))).simplified();
            if (label.isEmpty()) continue;
            const QString abs = navDir.absoluteFilePath(
                QUrl::fromPercentEncoding(ev.h.toUtf8()));
            m_toc.append({label, QUrl::fromLocalFile(abs), std::max(0, depth - 1)});
        }
    }
}

// ─── pageCount / pageSize ─────────────────────────────────────────────────────
int    EBookEngine::pageCount() const         { return m_spine.size(); }
QSizeF EBookEngine::pageSize(int /*p*/) const { return QSizeF(800.0, 1200.0); }
