#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QString>

class SidecarManager final : public QObject {
    Q_OBJECT
public:
    explicit SidecarManager(const QString& notesDir, QObject* parent = nullptr);

    // Chamado ao abrir um documento — configura o watcher no .md par
    void trackDocument(const QString& docPath);

    // Lê o sidecar atual (retorna vazio se não existe)
    [[nodiscard]] QString readSidecar() const;

    // Escreve/atualiza o sidecar sem disparar o watcher (via lock interno)
    void writeSidecar(const QString& markdown);

    // Persiste a última posição de leitura (capítulo EPUB ou página PDF)
    void savePagePosition(int page);

    // Recupera a última posição salva (-1 se não houver registro)
    [[nodiscard]] int loadPagePosition() const;

signals:
    // Emitido quando o arquivo .md é editado externamente (ex: Obsidian)
    void sidecarChangedExternally(const QString& newContent);

private slots:
    void onFileChanged(const QString& path);

private:
    [[nodiscard]] QString sidecarPath(const QString& docPath) const;
    [[nodiscard]] QString positionPath(const QString& docPath) const;

    QFileSystemWatcher m_watcher;
    QString            m_currentSidecar;
    QString            m_currentPositionFile;   // arquivo .pos paralelo ao .md
    QString            m_notesDir;
    bool               m_selfWriting = false;   // evita loop watcher ↔ write
};
