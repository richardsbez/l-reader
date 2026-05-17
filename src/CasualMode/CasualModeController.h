// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/CasualModeController.h  —  l-reader · Modo Casual
//
// Controlador QObject que expõe o estado da view Casual ao motor QML.
// Todas as propriedades são sincronizadas via Q_PROPERTY / NOTIFY,
// garantindo que a UI reaja automaticamente a qualquer mutação de estado.
//
// Integração: registar como context property antes de carregar o QML:
//   engine.rootContext()->setContextProperty("casualCtrl", &controller);
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>

class CasualModeController final : public QObject
{
    Q_OBJECT

    // ── Tema visual ──────────────────────────────────────────────────────────
    Q_PROPERTY(int     theme       READ theme       WRITE setTheme       NOTIFY themeChanged)

    // ── Cores derivadas do tema (lidas pelo QML via Binding) ─────────────────
    Q_PROPERTY(QString bgColor     READ bgColor     NOTIFY themeChanged)
    Q_PROPERTY(QString textColor   READ textColor   NOTIFY themeChanged)
    Q_PROPERTY(QString headerBg    READ headerBg    NOTIFY themeChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY themeChanged)
    Q_PROPERTY(QString borderColor READ borderColor NOTIFY themeChanged)
    Q_PROPERTY(QString mutedColor  READ mutedColor  NOTIFY themeChanged)

    // ── Estado de leitura ────────────────────────────────────────────────────
    Q_PROPERTY(QString chapterTitle    READ chapterTitle    WRITE setChapterTitle    NOTIFY chapterTitleChanged)
    Q_PROPERTY(QString bookTitle       READ bookTitle       CONSTANT)
    Q_PROPERTY(qreal   readingProgress READ readingProgress WRITE setReadingProgress NOTIFY readingProgressChanged)
    Q_PROPERTY(int     currentPage     READ currentPage     WRITE setCurrentPage     NOTIFY currentPageChanged)
    Q_PROPERTY(int     totalPages      READ totalPages      CONSTANT)

    // ── Tipografia / Layout ──────────────────────────────────────────────────
    Q_PROPERTY(int  fontSize     READ fontSize     WRITE setFontSize     NOTIFY fontSizeChanged)
    Q_PROPERTY(int  columnMargin READ columnMargin WRITE setColumnMargin NOTIFY columnMarginChanged)
    Q_PROPERTY(int  lineSpacing  READ lineSpacing  WRITE setLineSpacing  NOTIFY lineSpacingChanged)

    // ── Estado de painéis ────────────────────────────────────────────────────
    Q_PROPERTY(bool sidebarOpen READ sidebarOpen WRITE setSidebarOpen NOTIFY sidebarOpenChanged)
    Q_PROPERTY(bool searchOpen  READ searchOpen  WRITE setSearchOpen  NOTIFY searchOpenChanged)

    // ── Conteúdo mockado ─────────────────────────────────────────────────────
    Q_PROPERTY(QString leftPageHtml  READ leftPageHtml  CONSTANT)
    Q_PROPERTY(QString rightPageHtml READ rightPageHtml CONSTANT)

public:
    // Enumeração dos temas disponíveis — acessível diretamente no QML
    enum Theme {
        Light     = 0,  ///< Fundo branco, texto escuro  (#FAFAFA / #1A1A1A)
        Dark      = 1,  ///< Fundo escuro, texto claro   (#1E1E1E / #E8E8E8)
        Sepia     = 2,  ///< Creme quente, texto castanho (#F8F0E3 / #3B2D1F)
        Solarized = 3,  ///< Creme Solarized              (#FDF6E3 / #657B83)
    };
    Q_ENUM(Theme)

    explicit CasualModeController(QObject* parent = nullptr);

    // ── Getters ──────────────────────────────────────────────────────────────
    int     theme()          const noexcept { return static_cast<int>(m_theme); }
    QString chapterTitle()   const noexcept { return m_chapterTitle; }
    QString bookTitle()      const noexcept { return m_bookTitle; }
    qreal   readingProgress()const noexcept { return m_readingProgress; }
    int     currentPage()    const noexcept { return m_currentPage; }
    int     totalPages()     const noexcept { return m_totalPages; }
    int     fontSize()       const noexcept { return m_fontSize; }
    int     columnMargin()   const noexcept { return m_columnMargin; }
    int     lineSpacing()    const noexcept { return m_lineSpacing; }
    bool    sidebarOpen()    const noexcept { return m_sidebarOpen; }
    bool    searchOpen()     const noexcept { return m_searchOpen; }

    // Cores do tema actual
    QString bgColor()     const noexcept;
    QString textColor()   const noexcept;
    QString headerBg()    const noexcept;
    QString accentColor() const noexcept;
    QString borderColor() const noexcept;
    QString mutedColor()  const noexcept;

    // Conteúdo de página (HTML/RichText mockado)
    QString leftPageHtml()  const noexcept;
    QString rightPageHtml() const noexcept;

public slots:
    // ── Setters com emissão de sinal ─────────────────────────────────────────
    void setTheme(int theme);
    void setChapterTitle(const QString& title);
    void setReadingProgress(qreal progress);
    void setCurrentPage(int page);
    void setFontSize(int size);
    void setColumnMargin(int margin);
    void setLineSpacing(int spacing);
    void setSidebarOpen(bool open);
    void setSearchOpen(bool open);

    // ── Acções invocáveis pelo QML ────────────────────────────────────────────
    Q_INVOKABLE void increaseFontSize();
    Q_INVOKABLE void decreaseFontSize();
    Q_INVOKABLE void increaseMargin();
    Q_INVOKABLE void decreaseMargin();
    Q_INVOKABLE void toggleSidebar();
    Q_INVOKABLE void toggleSearch();

signals:
    void themeChanged();
    void chapterTitleChanged();
    void readingProgressChanged();
    void currentPageChanged();
    void fontSizeChanged();
    void columnMarginChanged();
    void lineSpacingChanged();
    void sidebarOpenChanged();
    void searchOpenChanged();

private:
    // ── Estado interno ───────────────────────────────────────────────────────
    Theme   m_theme          { Theme::Light };
    QString m_chapterTitle   { QStringLiteral("Capítulo III — O Labirinto Infinito") };
    QString m_bookTitle      { QStringLiteral("O Nome da Rosa") };
    qreal   m_readingProgress{ 0.04 };
    int     m_currentPage    { 12 };
    int     m_totalPages     { 300 };
    int     m_fontSize       { 17 };
    int     m_columnMargin   { 72 };   // px — margem lateral de cada coluna
    int     m_lineSpacing    { 8 };    // px extra acima de cada linha
    bool    m_sidebarOpen    { false };
    bool    m_searchOpen     { false };

    // ── Paletas de cor por tema ───────────────────────────────────────────────
    struct Palette {
        const char* bg;
        const char* text;
        const char* header;
        const char* accent;
        const char* border;
        const char* muted;
    };
    static constexpr Palette kPalettes[4] = {
        /* Light     */ { "#FAFAFA", "#1A1A1A", "#F5F5F5", "#CC0000", "#E0E0E0", "#888888" },
        /* Dark      */ { "#1E1E1E", "#E8E8E8", "#252525", "#CF6679", "#3A3A3A", "#909090" },
        /* Sepia     */ { "#F8F0E3", "#3B2D1F", "#F0E6D3", "#8B6042", "#D4C4A8", "#9A8070" },
        /* Solarized */ { "#FDF6E3", "#657B83", "#EEE8D5", "#268BD2", "#D7CFBC", "#93A1A1" },
    };

    // ── Texto mockado (gerado uma vez, partilhado entre instâncias) ───────────
    static const QString& staticLeftHtml();
    static const QString& staticRightHtml();
};
