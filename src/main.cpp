// main.cpp
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include "ViewLayer/main_window.hpp"

int main(int argc, char* argv[])
{
    // ── Metadados da aplicação ────────────────────────────────────────────────
    // Usados por QStandardPaths (AppDataLocation) e pela barra de título do OS.
    QApplication::setApplicationName(QStringLiteral("l-reader"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("l-reader"));

    // QWebEngine exige QApplication (não QCoreApplication nem QGuiApplication)
    QApplication app(argc, argv);

    // ── CLI ───────────────────────────────────────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Leitor de documentos — PDF, EPUB, MOBI e Markdown"));
    parser.addHelpOption();
    parser.addVersionOption();

    // Argumento posicional opcional: caminho do arquivo a abrir
    parser.addPositionalArgument(
        QStringLiteral("arquivo"),
        QStringLiteral("Documento a abrir (PDF, EPUB, MOBI ou Markdown)"),
        QStringLiteral("[arquivo]"));

    parser.process(app);

    // ── Janela principal ──────────────────────────────────────────────────────
    MainWindow window;
    window.setWindowTitle(QStringLiteral("l-reader"));
    window.resize(1024, 768);
    window.show();

    // Se um arquivo foi passado na linha de comando, abre imediatamente
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        window.openDocument(positional.first());

    return app.exec();
}
