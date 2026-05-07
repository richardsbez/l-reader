#include "sidecar_manager.hpp"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QSettings>

SidecarManager::SidecarManager(const QString& notesDir, QObject* parent)
    : QObject(parent), m_notesDir(notesDir)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this,       &SidecarManager::onFileChanged);
}

void SidecarManager::trackDocument(const QString& docPath)
{
    if (!m_currentSidecar.isEmpty())
        m_watcher.removePath(m_currentSidecar);

    m_currentSidecar      = sidecarPath(docPath);
    m_currentPositionFile = positionPath(docPath);

    // Cria o arquivo de notas se não existe
    if (!QFile::exists(m_currentSidecar)) {
        const QFileInfo info(docPath);
        QFile f(m_currentSidecar);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream s(&f);
            s << QStringLiteral("# Notes: %1\n\n").arg(info.fileName());
        }
    }

    m_watcher.addPath(m_currentSidecar);
}

QString SidecarManager::readSidecar() const
{
    QFile f(m_currentSidecar);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&f);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

void SidecarManager::writeSidecar(const QString& markdown)
{
    m_selfWriting = true;   // suprime o onFileChanged durante a escrita
    QFile f(m_currentSidecar);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        QTextStream(&f) << markdown;
    m_selfWriting = false;
}

// ─── savePagePosition / loadPagePosition ─────────────────────────────────────
//
// Usa um arquivo .pos (texto simples com um inteiro) ao lado do .md.
// Exemplo:  ~/.local/share/l-reader/notes/meulivro.pos
//
void SidecarManager::savePagePosition(int page)
{
    if (m_currentPositionFile.isEmpty()) return;

    QFile f(m_currentPositionFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        QTextStream(&f) << page;
}

int SidecarManager::loadPagePosition() const
{
    if (m_currentPositionFile.isEmpty()) return -1;

    QFile f(m_currentPositionFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    bool ok = false;
    const int page = QTextStream(&f).readAll().trimmed().toInt(&ok);
    return ok ? page : -1;
}

// ─── Slots / internos ────────────────────────────────────────────────────────

void SidecarManager::onFileChanged(const QString& path)
{
    if (m_selfWriting || path != m_currentSidecar) return;
    // Re-adiciona ao watcher: alguns editores fazem delete+create
    m_watcher.addPath(path);
    emit sidecarChangedExternally(readSidecar());
}

QString SidecarManager::sidecarPath(const QString& docPath) const
{
    const QFileInfo info(docPath);
    return QDir(m_notesDir).filePath(info.completeBaseName()
                                     + QStringLiteral(".md"));
}

QString SidecarManager::positionPath(const QString& docPath) const
{
    const QFileInfo info(docPath);
    return QDir(m_notesDir).filePath(info.completeBaseName()
                                     + QStringLiteral(".pos"));
}
