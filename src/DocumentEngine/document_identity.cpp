// document_identity.cpp
// ─────────────────────────────────────────────────────────────────────────────
// Implementação do "RG" de arquivo: SHA-1 dos primeiros 64 KB + tamanho.
//
// Por que 64 KB?
//   • Rápido mesmo em HDDs lentos (< 1 ms na prática).
//   • Suficiente para distinguir virtualmente qualquer par de PDFs diferentes.
//   • Não carrega o arquivo inteiro (PDFs podem ter centenas de MB).
//
// Por que incluir o tamanho?
//   • Dois arquivos com header idêntico mas tamanhos diferentes
//     (ex.: versões diferentes do mesmo documento) recebem IDs diferentes.
// ─────────────────────────────────────────────────────────────────────────────
#include "document_identity.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

// ─────────────────────────────────────────────────────────────────────────────
// Cache thread-safe
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    QHash<QString, QString> s_cache;
    QMutex                  s_mutex;

    constexpr qint64 kHeaderBytes = 65'536;   // 64 KB
}

// ─────────────────────────────────────────────────────────────────────────────
// computeId — trabalho real de hashing
// ─────────────────────────────────────────────────────────────────────────────
QString DocumentIdentity::computeId(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hasher(QCryptographicHash::Sha1);

    // 1) Primeiros 64 KB de conteúdo
    const QByteArray header = f.read(kHeaderBytes);
    if (header.isEmpty())
        return {};
    hasher.addData(header);

    // 2) Tamanho total do arquivo (como bytes big-endian de 8 bytes)
    const qint64 size = QFileInfo(filePath).size();
    QByteArray sizeBytes(8, '\0');
    for (int i = 7; i >= 0; --i) {
        sizeBytes[i] = static_cast<char>(size & 0xFF);
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        const_cast<qint64&>(size); // suppress warning — just shifting
    }
    // Serialização manual big-endian para evitar dependência de platform
    qint64 tmp = size;
    for (int i = 7; i >= 0; --i) {
        sizeBytes[i] = static_cast<char>(tmp & 0xFF);
        tmp >>= 8;
    }
    hasher.addData(sizeBytes);

    // Retorna os primeiros 16 hex chars (64 bits de entropia — mais que suficiente)
    return QString::fromLatin1(hasher.result().toHex()).left(16);
}

// ─────────────────────────────────────────────────────────────────────────────
// idForFile — ponto de entrada público com cache
// ─────────────────────────────────────────────────────────────────────────────
QString DocumentIdentity::idForFile(const QString& filePath)
{
    {
        QMutexLocker lock(&s_mutex);
        const auto it = s_cache.constFind(filePath);
        if (it != s_cache.constEnd())
            return it.value();
    }

    const QString id = computeId(filePath);

    if (!id.isEmpty()) {
        QMutexLocker lock(&s_mutex);
        s_cache.insert(filePath, id);
    }

    return id;
}

// ─────────────────────────────────────────────────────────────────────────────
// invalidate / clearCache
// ─────────────────────────────────────────────────────────────────────────────
void DocumentIdentity::invalidate(const QString& filePath)
{
    QMutexLocker lock(&s_mutex);
    s_cache.remove(filePath);
}

void DocumentIdentity::clearCache()
{
    QMutexLocker lock(&s_mutex);
    s_cache.clear();
}
