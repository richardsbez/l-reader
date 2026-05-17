// document_identity.hpp
// ─────────────────────────────────────────────────────────────────────────────
// DocumentIdentity — "RG" estável de um arquivo.
//
// Inspirado na lógica do Okular: em vez de usar o nome do arquivo como chave
// (quebraria ao mover de "Downloads/" para "Documentos/"), calculamos uma
// impressão digital dos primeiros 64 KB de conteúdo + tamanho total.
//
// Resultado: o sidecar JSON é encontrado mesmo após renomear ou mover o PDF.
//
// Uso:
//   const QString id = DocumentIdentity::idForFile("/home/user/livro.pdf");
//   // "a3f2c1b8e4d09712"  ← 16 hex chars, estável para o mesmo arquivo
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QString>

class DocumentIdentity final {
public:
    DocumentIdentity() = delete;   // apenas funções estáticas

    // ── API principal ──────────────────────────────────────────────────────

    // Retorna um ID de 16 hex chars baseado no conteúdo do arquivo.
    // Usa cache em memória; chamadas repetidas para o mesmo path são gratuitas.
    // Retorna string vazia se o arquivo não puder ser lido.
    [[nodiscard]] static QString idForFile(const QString& filePath);

    // Limpa o cache (útil em testes ou quando o arquivo foi substituído).
    static void invalidate(const QString& filePath);
    static void clearCache();

private:
    [[nodiscard]] static QString computeId(const QString& filePath);
};
