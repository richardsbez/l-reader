// main_window.cpp  —  l-reader
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  Ponto de entrada da MainWindow.                                        │
// │  Responsabilidades deste arquivo:                                       │
// │    • Construtor + destruição                                            │
// │    • wireSignals() — orquestra os wire helpers de cada subsistema       │
// │    • Diálogo de arquivo (D-Bus XDG portal)                              │
// │    • Hot-reload de QSS (applySS)                                        │
// │    • Lifecycle: closeEvent, autoSaveToPdf                               │
// │    • Utilitários gerais: setMargins, notesDir                           │
// └─────────────────────────────────────────────────────────────────────────┘

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"
#include "DocumentEngine/pdf_engine.hpp"
#include "DocumentEngine/ebook_engine.hpp"
#include "DocumentEngine/toc_worker.hpp"
#include "DocumentEngine/pdf_bookmark_manager.hpp"
#include "bookmark_item_widget.hpp"

#include "Ui/layout_tokens.hpp"
#include "Ui/ui_strings.hpp"
#include "Ui/epub_style.hpp"

#include <QApplication>
#include <QActionGroup>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QStatusBar>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QUrl>
#include <QDebug>
#include <QFileSystemWatcher>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    buildActions();
    buildMenuBar();
    buildToolBarActions();
    buildAnnotDock();
    buildBottomNav();
    buildChapterHeader();
    buildStatusBar();

    m_modeManager     = std::make_unique<ModeManager>(this, this);
    m_pageCache       = std::make_unique<PageCache>(this);
    m_sidecar         = std::make_unique<SidecarManager>(notesDir(), this);
    m_tocWorker       = std::make_unique<TocWorker>(this);
    m_bookmarkManager  = std::make_unique<PdfBookmarkManager>(this);
    m_highlightManager = std::make_unique<HighlightManager>(this);

    wireSignals();
}

// ─────────────────────────────────────────────────────────────────────────────
// wireSignals — delega para os wire helpers de cada subsistema.
//
// A ordem importa: subsistemas de nível mais baixo (scroll, página) primeiro;
// subsistemas que dependem deles (documento, modo) por último.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireSignals()
{
    wireScrollSignals();     // main_window_scroll.cpp
    wirePageZoomSignals();   // main_window_page.cpp
    wireEpubSignals();       // main_window_epub.cpp
    wireTocSignals();        // main_window_toc.cpp
    wireBookmarkSignals();   // main_window_bookmarks.cpp
    wireHighlightSignals();  // main_window_bookmarks.cpp
    wireGallerySignals();    // main_window_gallery.cpp
    wireModeSignals();       // main_window_document.cpp
    wireDocumentSignals();   // main_window_document.cpp
}

// ─────────────────────────────────────────────────────────────────────────────
// D-Bus serialization (PortalFilterRule / PortalFilter)
// ─────────────────────────────────────────────────────────────────────────────
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilterRule &rule) {
    arg.beginStructure(); arg << rule.type << rule.pattern; arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilterRule &rule) {
    arg.beginStructure(); arg >> rule.type >> rule.pattern; arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilter &filter) {
    arg.beginStructure(); arg << filter.name << filter.rules; arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilter &filter) {
    arg.beginStructure(); arg >> filter.name >> filter.rules; arg.endStructure();
    return arg;
}

// ─────────────────────────────────────────────────────────────────────────────
// openFileDialog — abre o XDG FileChooser portal via D-Bus
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openFileDialog()
{
    qDBusRegisterMetaType<PortalFilterRule>();
    qDBusRegisterMetaType<QList<PortalFilterRule>>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<QList<PortalFilter>>();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        u"org.freedesktop.portal.Desktop"_s,
        u"/org/freedesktop/portal/desktop"_s,
        u"org.freedesktop.portal.FileChooser"_s,
        u"OpenFile"_s);

    QVariantMap options;
    options[u"multiple"_s] = false;
    QList<PortalFilter> filters = {
        {tr("Documentos"),
            {{0, u"*.pdf"_s},{0, u"*.epub"_s},{0, u"*.mobi"_s},
             {0, u"*.azw"_s},{0, u"*.md"_s},{0, u"*.markdown"_s}}},
        {tr("PDF (*.pdf)"),  {{0, u"*.pdf"_s}}},
        {tr("EPUB (*.epub)"),{{0, u"*.epub"_s}}},
        {tr("MOBI (*.mobi)"),{{0, u"*.mobi"_s},{0, u"*.azw"_s}}},
        {tr("Markdown"),     {{0, u"*.md"_s},{0, u"*.markdown"_s}}},
        {tr("Todos (*)"),    {{0, u"*"_s}}}
    };
    options[u"filters"_s] = QVariant::fromValue(filters);
    msg << u""_s << tr("Abrir documento") << options;

    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qWarning() << "Portal D-Bus error:" << reply.error().message();
            return;
        }
        QDBusConnection::sessionBus().connect(
            u"org.freedesktop.portal.Desktop"_s,
            reply.value().path(),
            u"org.freedesktop.portal.Request"_s,
            u"Response"_s,
            this, SLOT(onPortalResponse(uint, QVariantMap)));
    });
}

void MainWindow::onPortalResponse(uint response, const QVariantMap &results)
{
    if (response == 0 && results.contains(u"uris"_s)) {
        const QStringList uris = results[u"uris"_s].toStringList();
        if (!uris.isEmpty())
            openDocument(QUrl(uris.first()).toLocalFile());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// applySS — aplica QSS e habilita hot-reload via QFileSystemWatcher
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::applySS(const QString& resourcePath)
{
    if (!m_cssWatcher) {
        m_cssWatcher = new QFileSystemWatcher(this);
        connect(m_cssWatcher, &QFileSystemWatcher::fileChanged,
                this, [this](const QString& changedPath) {
            if (!m_cssWatcher->files().contains(changedPath))
                m_cssWatcher->addPath(changedPath);
            QFile f(changedPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                setStyleSheet(QString::fromUtf8(f.readAll()));
        });
    }

    const QString relPath = QString(resourcePath).remove(0, 2);
    const QString execDir = QCoreApplication::applicationDirPath();
    QString diskPath;
    for (const QString& c : QStringList{
            execDir + u'/' + relPath,
            execDir + QStringLiteral("/styles/") + QFileInfo(relPath).fileName()
         })
    {
        if (QFile::exists(c)) { diskPath = c; break; }
    }

    if (!m_currentSsPath.isEmpty())
        m_cssWatcher->removePath(m_currentSsPath);

    if (!diskPath.isEmpty()) {
        QFile f(diskPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(f.readAll()));
        m_cssWatcher->addPath(diskPath);
        m_currentSsPath = diskPath;
    } else {
        QFile f(resourcePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(f.readAll()));
        m_currentSsPath.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setMargins / notesDir
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setMargins(const QMargins& m)
{
    if (m_pdfScroll) m_pdfScroll->setContentsMargins(m);
    if (m_webView)   m_webView->setContentsMargins(m);
}

QString MainWindow::notesDir() const
{
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::AppDataLocation)
                        + QStringLiteral("/notes");
    QDir().mkpath(dir);
    return dir;
}

// ─────────────────────────────────────────────────────────────────────────────
// autoSaveToPdf — embute highlights/marcadores no PDF (estilo Okular)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::autoSaveToPdf()
{
    if (m_highlightManager && m_highlightManager->isPdfOutOfSync())
        m_highlightManager->saveToPdf();

    if (m_bookmarkManager && m_bookmarkManager->isPdfOutOfSync())
        m_bookmarkManager->saveToPdf();
}

// ─────────────────────────────────────────────────────────────────────────────
// closeEvent — ao fechar, embute anotações no PDF (estilo Okular)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::closeEvent(QCloseEvent* event)
{
    autoSaveToPdf();
    QMainWindow::closeEvent(event);
}
