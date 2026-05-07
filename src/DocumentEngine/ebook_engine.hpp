// ebook_engine.hpp
// ─────────────────────────────────────────────────────────────────────────────
// EBookEngine — backend para EPUB (e MOBI via Calibre).
//
// Responsabilidades:
//   1. Extrair o ZIP do EPUB em diretório temporário (via `unzip`)
//   2. Parsear META-INF/container.xml → localizar o OPF
//   3. Parsear o OPF  → spine (ordem de leitura) + manifest + metadados
//   4. Parsear o NCX (EPUB2) ou NAV (EPUB3) → TOC hierárquico
//   5. Emitir documentLoaded() de forma SÍNCRONA após o parse
//
// O que NÃO é responsabilidade deste engine:
//   - Qualquer coisa visual / Qt Widgets
//   - QWebEnginePage / QWebEngineView
//   - CSS injection ou paginação por colunas
//
// Toda a renderização é delegada ao QWebEngineView na MainWindow.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include "document_engine.hpp"   // TocEntry definida aqui
#include <QDir>
#include <QList>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────

class EBookEngine final : public DocumentEngine {
    Q_OBJECT
public:
    explicit EBookEngine(QObject* parent = nullptr);

    bool         load(const QString& path) override;
    int          pageCount()               const override;
    QSizeF       pageSize(int /*page*/)    const override;
    DocumentType type()                    const override { return m_type; }

    // ── Sumário — implementação da interface base ────────────────────────────
    [[nodiscard]] QList<TocEntry> tocEntries() const override { return m_toc; }

    // ── Acesso à spine (cada item = um "capítulo") ──────────────────────────
    [[nodiscard]] const QList<QUrl>& spineUrls() const { return m_spine;  }
    [[nodiscard]] QString            title()     const { return m_title;  }
    [[nodiscard]] QString            author()    const { return m_author; }

private:
    // ── Parse pipeline ───────────────────────────────────────────────────────
    bool extractZip(const QString& zipPath, const QString& destDir);
    bool parseContainer(const QString& extractDir, QString& outOpfPath);
    bool parseOPF(const QString& opfPath);
    void parseNCX(const QString& ncxPath, const QDir& opfDir);
    void parseNAV(const QString& navPath, const QDir& opfDir);

    // ── State ────────────────────────────────────────────────────────────────
    DocumentType    m_type   = DocumentType::EPUB;
    QList<QUrl>     m_spine;       // URLs locais na ordem de leitura
    QList<TocEntry> m_toc;         // entradas do sumário
    QString         m_title;
    QString         m_author;
};
