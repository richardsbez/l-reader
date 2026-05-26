// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/casual_mode_controller.hpp  —  l-reader · Modo Casual
//
// Controlador QObject que expõe o estado do documento ao motor QML.
// Todas as propriedades são sincronizadas via Q_PROPERTY / NOTIFY.
//
// ── Uso ──────────────────────────────────────────────────────────────────────
//   // Ao abrir um documento ou trocar para modo Casual:
//   controller->setDocument(engine, currentPage);
//
//   // Ao mudar de página (slot conectado a MainWindow::onPageChanged):
//   controller->setCurrentPage(page);       // PDF  — usa índice 0-based
//   controller->setCurrentChapterIndex(i);  // EPUB — usa índice da spine
//
//   // No QML (casual_mode_view.qml) o conteúdo EPUB está em:
//   casualCtrl.chapterHtml   // HTML do capítulo actual
//
// ── Dependências ─────────────────────────────────────────────────────────────
//   DocumentEngine* — ponteiro não-owning; gerido por MainWindow.
//   EBookEngine*    — cast estático usado em setDocument() (arquivo .cpp
//   apenas).
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

// Forward declarations — evitam incluir headers pesados aqui
class DocumentEngine;

class CasualModeController final : public QObject {
  Q_OBJECT

  // ── Documento ────────────────────────────────────────────────────────────
  Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
  Q_PROPERTY(QString bookTitle READ bookTitle NOTIFY documentChanged)
  Q_PROPERTY(QString author READ author NOTIFY documentChanged)

  // ── Tema visual ──────────────────────────────────────────────────────────
  Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY themeChanged)

  // ── Cores derivadas do tema ───────────────────────────────────────────────
  Q_PROPERTY(QString bgColor READ bgColor NOTIFY themeChanged)
  Q_PROPERTY(QString textColor READ textColor NOTIFY themeChanged)
  Q_PROPERTY(QString headerBg READ headerBg NOTIFY themeChanged)
  Q_PROPERTY(QString accentColor READ accentColor NOTIFY themeChanged)
  Q_PROPERTY(QString borderColor READ borderColor NOTIFY themeChanged)
  Q_PROPERTY(QString mutedColor READ mutedColor NOTIFY themeChanged)

  // ── Estado de leitura ────────────────────────────────────────────────────
  Q_PROPERTY(QString chapterTitle READ chapterTitle NOTIFY chapterTitleChanged)
  Q_PROPERTY(
      qreal readingProgress READ readingProgress NOTIFY readingProgressChanged)
  Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)
  Q_PROPERTY(int totalPages READ totalPages NOTIFY documentChanged)

  // ── Conteúdo EPUB ────────────────────────────────────────────────────────
  // chapterHtml : corpo HTML do capítulo actual (extraído do spine item).
  //               Vazio para documentos PDF — o QML usa PdfCanvasView nesses.
  // chapterUrl  : URL do ficheiro no disco (para debug / WebEngineView futuro).
  Q_PROPERTY(QString chapterHtml READ chapterHtml NOTIFY chapterChanged)
  Q_PROPERTY(QUrl chapterUrl READ chapterUrl NOTIFY chapterChanged)
  Q_PROPERTY(int chapterIndex READ chapterIndex NOTIFY chapterChanged)
  Q_PROPERTY(int chapterCount READ chapterCount NOTIFY documentChanged)

  // ── Posições de break de capítulo para o footer (proporções 0..1) ────────
  // Calculado em setDocument() a partir do TOC; reactivo no QML.
  Q_PROPERTY(QVariantList chapterBreakPositions READ chapterBreakPositions
                 NOTIFY documentChanged)

  // ── Tipografia / Layout ──────────────────────────────────────────────────
  Q_PROPERTY(
      int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
  Q_PROPERTY(int columnMargin READ columnMargin WRITE setColumnMargin NOTIFY
                 columnMarginChanged)
  Q_PROPERTY(int lineSpacing READ lineSpacing WRITE setLineSpacing NOTIFY
                 lineSpacingChanged)

  // ── Estado de painéis ────────────────────────────────────────────────────
  Q_PROPERTY(bool sidebarOpen READ sidebarOpen WRITE setSidebarOpen NOTIFY
                 sidebarOpenChanged)
  Q_PROPERTY(bool searchOpen READ searchOpen WRITE setSearchOpen NOTIFY
                 searchOpenChanged)

public:
  enum Theme {
    Light = 0,
    Dark = 1,
    Sepia = 2,
    Solarized = 3,
  };
  Q_ENUM(Theme)

  explicit CasualModeController(QObject *parent = nullptr);

  // ── Getters ──────────────────────────────────────────────────────────────
  bool hasDocument() const noexcept { return m_hasDocument; }
  int theme() const noexcept { return static_cast<int>(m_theme); }
  QString bookTitle() const noexcept { return m_bookTitle; }
  QString author() const noexcept { return m_author; }
  QString chapterTitle() const noexcept { return m_chapterTitle; }
  qreal readingProgress() const noexcept { return m_readingProgress; }
  int currentPage() const noexcept { return m_currentPage; }
  int totalPages() const noexcept { return m_totalPages; }
  QString chapterHtml() const noexcept { return m_chapterHtml; }
  QUrl chapterUrl() const noexcept { return m_chapterUrl; }
  int chapterIndex() const noexcept { return m_chapterIndex; }
  int chapterCount() const noexcept { return m_chapterCount; }
  int fontSize() const noexcept { return m_fontSize; }
  int columnMargin() const noexcept { return m_columnMargin; }
  int lineSpacing() const noexcept { return m_lineSpacing; }
  bool sidebarOpen() const noexcept { return m_sidebarOpen; }
  bool searchOpen() const noexcept { return m_searchOpen; }

  QVariantList chapterBreakPositions() const noexcept {
    return m_chapterBreaks;
  }

  // Cores do tema
  QString bgColor() const noexcept;
  QString textColor() const noexcept;
  QString headerBg() const noexcept;
  QString accentColor() const noexcept;
  QString borderColor() const noexcept;
  QString mutedColor() const noexcept;

public slots:
  // ── Alimentação de dados — chamados pela MainWindow ───────────────────────

  // Ponto de entrada principal.  Deve ser chamado sempre que:
  //   a) um novo documento é aberto (qualquer modo activo), ou
  //   b) o utilizador entra no Modo Casual com um documento já aberto.
  //
  // engine     : ponteiro não-owning (gerido por MainWindow::m_engine)
  // startPage  : página PDF (0-based) ou índice spine EPUB a mostrar
  void setDocument(DocumentEngine *engine, int startPage = 0);

  // Sincronização de página — chamar de MainWindow::onPageChanged().
  // Para PDF  : page é 0-based.
  // Para EPUB : page é o índice da spine (m_currentChapter no MainWindow).
  void setCurrentPage(int page);

  // Permite navegar directamente para um índice de capítulo (EPUB).
  // Emite chapterNavigationRequested() para que MainWindow carregue o URL.
  Q_INVOKABLE void setCurrentChapterIndex(int index);

  // Teclas de navegação de capítulo (botões do footer QML)
  Q_INVOKABLE void requestNextChapter();
  Q_INVOKABLE void requestPrevChapter();

  // Setters de tipografia / tema
  void setTheme(int theme);
  void setFontSize(int size);
  void setColumnMargin(int margin);
  void setLineSpacing(int spacing);
  void setSidebarOpen(bool open);
  void setSearchOpen(bool open);

  // Acções QML
  Q_INVOKABLE void increaseFontSize();
  Q_INVOKABLE void decreaseFontSize();
  Q_INVOKABLE void increaseMargin();
  Q_INVOKABLE void decreaseMargin();
  Q_INVOKABLE void toggleSidebar();
  Q_INVOKABLE void toggleSearch();

signals:
  // ── Dados ────────────────────────────────────────────────────────────────
  void documentChanged();
  void chapterChanged();
  void chapterTitleChanged();
  void readingProgressChanged();
  void currentPageChanged();
  void themeChanged();
  void fontSizeChanged();
  void columnMarginChanged();
  void lineSpacingChanged();
  void sidebarOpenChanged();
  void searchOpenChanged();

  // ── Comandos para MainWindow ──────────────────────────────────────────────
  // Emitido por requestNextChapter/prevChapter e setCurrentChapterIndex().
  // MainWindow conecta este sinal e chama loadEpubChapter(index).
  void chapterNavigationRequested(int chapterIndex);

private:
  // ── Helpers internos ──────────────────────────────────────────────────────
  void syncMetadataFromEngine();
  void computeChapterBreaks();

  // Carrega o HTML de m_spineUrls[chapterIndex] e popula m_chapterHtml.
  // Só actualiza se o índice mudou.
  void loadChapterContent(int chapterIndex);

  // Extrai o conteúdo do <body> de um documento HTML.
  static QString extractBodyContent(const QString &fullHtml);

  // Procura no TOC o título correspondente ao índice de capítulo EPUB
  // ou à página PDF.
  void updateChapterTitle(int pageOrIndex);

  void setReadingProgress(qreal progress);
  void setChapterTitle(const QString &title);

  // ── Paletas de cor por tema ───────────────────────────────────────────────
  struct Palette {
    const char *bg;
    const char *text;
    const char *header;
    const char *accent;
    const char *border;
    const char *muted;
  };
  static constexpr Palette kPalettes[4] = {
      /* Light     */ {"#FAFAFA", "#1A1A1A", "#F5F5F5", "#CC0000", "#E0E0E0",
                       "#888888"},
      /* Dark      */
      {"#1E1E1E", "#E8E8E8", "#252525", "#CF6679", "#3A3A3A", "#909090"},
      /* Sepia     */
      {"#F8F0E3", "#3B2D1F", "#F0E6D3", "#8B6042", "#D4C4A8", "#9A8070"},
      /* Solarized */
      {"#FDF6E3", "#657B83", "#EEE8D5", "#268BD2", "#D7CFBC", "#93A1A1"},
  };

  // ── Estado do documento ───────────────────────────────────────────────────
  DocumentEngine *m_engine = nullptr; // non-owning
  bool m_hasDocument = false;
  QString m_bookTitle;
  QString m_author;
  int m_totalPages = 0;
  int m_chapterCount = 0;

  // Spine EPUB (vazio para PDF)
  QList<QUrl> m_spineUrls;

  // TOC — usado para resolver títulos de capítulo
  struct TocItem {
    QString title;
    QUrl url;
    int page = -1;
  };
  QList<TocItem> m_toc;

  // Posições 0..1 dos inícios de capítulo (para o footer)
  QVariantList m_chapterBreaks;

  // ── Estado de leitura ─────────────────────────────────────────────────────
  Theme m_theme{Theme::Light};
  QString m_chapterTitle;
  qreal m_readingProgress{0.0};
  int m_currentPage{0};
  int m_chapterIndex{0};

  // Conteúdo HTML do capítulo actual (apenas EPUB)
  QString m_chapterHtml;
  QUrl m_chapterUrl;
  int m_loadedChapterIndex{-1}; // índice já em cache — evita releituras

  // ── Tipografia ────────────────────────────────────────────────────────────
  int m_fontSize{17};
  int m_columnMargin{72};
  int m_lineSpacing{8};

  // ── Painéis ───────────────────────────────────────────────────────────────
  bool m_sidebarOpen{false};
  bool m_searchOpen{false};
};
