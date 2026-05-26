// ─────────────────────────────────────────────────────────────────────────────
// src/CasualMode/casual_mode_controller.cpp  —  l-reader · Modo Casual
// ─────────────────────────────────────────────────────────────────────────────
#include "casual_mode_controller.hpp"

#include "DocumentEngine/document_engine.hpp"
#include "DocumentEngine/ebook_engine.hpp"

#include <QFile>
#include <QRegularExpression>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Definição out-of-class da constexpr — removida (C++17 inline implícito).
// Em C++20 (padrão do projecto) membros static constexpr não precisam de
// definição fora da classe; deixar a linha causaria violação de ODR no Unity
// Build.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
CasualModeController::CasualModeController(QObject *parent) : QObject(parent) {}

// ─────────────────────────────────────────────────────────────────────────────
// setDocument — ponto de entrada principal
//
// Deve ser chamado sempre que:
//   • um documento é aberto (openDocument no MainWindow), OU
//   • o utilizador entra no Modo Casual com um documento já carregado.
//
// startPage: índice 0-based de página PDF, ou índice de spine para EPUB.
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setDocument(DocumentEngine *engine, int startPage) {
  m_engine = engine;
  m_hasDocument = (engine != nullptr);
  m_loadedChapterIndex = -1; // invalida o cache de HTML

  m_spineUrls.clear();
  m_toc.clear();
  m_chapterBreaks.clear();
  m_bookTitle.clear();
  m_author.clear();
  m_totalPages = 0;
  m_chapterCount = 0;
  m_chapterHtml.clear();
  m_chapterUrl = QUrl{};

  if (!engine) {
    emit documentChanged();
    return;
  }

  syncMetadataFromEngine();

  // Emite tudo de uma vez — a UI actualiza num único frame
  emit documentChanged();

  // Posiciona no ponto de leitura inicial
  setCurrentPage(startPage);
}

// ─────────────────────────────────────────────────────────────────────────────
// syncMetadataFromEngine — extrai título, autor, spine, TOC e page count.
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::syncMetadataFromEngine() {
  const DocumentType dt = m_engine->type();

  // ── EPUB / MOBI ───────────────────────────────────────────────────────────
  if (dt == DocumentType::EPUB || dt == DocumentType::MOBI) {
    auto *ebook = static_cast<EBookEngine *>(m_engine);

    m_bookTitle = ebook->title();
    m_author = ebook->author();
    m_spineUrls = ebook->spineUrls();
    m_chapterCount = m_spineUrls.size();
    m_totalPages = m_chapterCount; // "páginas" = capítulos no Casual EPUB

    // Popula o TOC interno a partir do DocumentEngine genérico
    for (const TocEntry &e : ebook->tocEntries()) {
      m_toc.append({e.title, e.url, e.page});
    }
  }
  // ── PDF ───────────────────────────────────────────────────────────────────
  else if (dt == DocumentType::PDF) {
    m_totalPages = m_engine->pageCount();
    m_chapterCount = 0; // PDF usa páginas, não capítulos discretos

    for (const TocEntry &e : m_engine->tocEntries()) {
      m_toc.append({e.title, {}, e.page});
    }
  }
  // ── Markdown ──────────────────────────────────────────────────────────────
  else {
    m_totalPages = m_engine->pageCount();
  }

  computeChapterBreaks();
}

// ─────────────────────────────────────────────────────────────────────────────
// computeChapterBreaks — calcula as proporções 0..1 para os ticks do footer.
//
// Para EPUB: divide os capítulos de forma uniforme (sem informação de tamanho).
// Para PDF : usa as páginas do TOC.
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::computeChapterBreaks() {
  m_chapterBreaks.clear();
  if (m_totalPages <= 0)
    return;

  if (!m_spineUrls.isEmpty()) {
    // EPUB — capítulos igualmente espaçados
    for (int i = 0; i < m_chapterCount; ++i)
      m_chapterBreaks.append(QVariant(static_cast<qreal>(i) / m_chapterCount));
    m_chapterBreaks.append(QVariant(1.0));
  } else {
    // PDF — usa páginas do TOC para breaks reais
    for (const TocItem &t : m_toc) {
      if (t.page >= 0)
        m_chapterBreaks.append(
            QVariant(static_cast<qreal>(t.page) / m_totalPages));
    }
    if (!m_chapterBreaks.isEmpty())
      m_chapterBreaks.append(QVariant(1.0));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// setCurrentPage — sincronizado a partir de MainWindow::onPageChanged()
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setCurrentPage(int page) {
  if (m_totalPages <= 0)
    return;

  const int p = std::clamp(page, 0, m_totalPages - 1);
  if (m_currentPage == p && m_hasDocument)
    return;
  m_currentPage = p;

  setReadingProgress(static_cast<qreal>(p + 1) / m_totalPages);
  updateChapterTitle(p);

  // Para EPUB, sincroniza também o índice de capítulo e o HTML
  if (!m_spineUrls.isEmpty())
    loadChapterContent(p);

  emit currentPageChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// setCurrentChapterIndex — navegação directa por capítulo (QML)
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setCurrentChapterIndex(int index) {
  if (m_spineUrls.isEmpty())
    return;
  const int i = std::clamp(index, 0, m_chapterCount - 1);
  if (m_chapterIndex == i)
    return;
  m_chapterIndex = i;

  // Pede ao MainWindow que carregue o capítulo (não toca em QWebEngineView
  // aqui)
  emit chapterNavigationRequested(i);
}

// ─────────────────────────────────────────────────────────────────────────────
// requestNextChapter / requestPrevChapter — botões do footer QML
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::requestNextChapter() {
  setCurrentChapterIndex(m_chapterIndex + 1);
}

void CasualModeController::requestPrevChapter() {
  setCurrentChapterIndex(m_chapterIndex - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadChapterContent — lê o HTML do ficheiro de spine e popula m_chapterHtml.
//
// O índice é o índice na spine EPUB (= m_currentPage para EPUB).
// Faz cache do último índice carregado — re-leitura desnecessária em
// chamadas consecutivas para a mesma página.
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::loadChapterContent(int chapterIndex) {
  if (chapterIndex < 0 || chapterIndex >= m_spineUrls.size())
    return;
  if (chapterIndex == m_loadedChapterIndex)
    return; // já em cache

  m_loadedChapterIndex = chapterIndex;
  m_chapterIndex = chapterIndex;
  m_chapterUrl = m_spineUrls.at(chapterIndex);

  QFile f(m_chapterUrl.toLocalFile());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    m_chapterHtml = QStringLiteral("<p>Erro ao carregar capítulo.</p>");
    emit chapterChanged();
    return;
  }

  const QString fullHtml = QString::fromUtf8(f.readAll());
  m_chapterHtml = extractBodyContent(fullHtml);
  emit chapterChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// extractBodyContent — extrai o conteúdo do <body> de um documento HTML.
//
// O QRegularExpression usa DotMatchesEverything para lidar com HTML
// multiline.  A captura é não-greedy (.*?) para parar no primeiro </body>,
// necessário quando o documento contém body tags aninhadas (raro mas possível
// em EPUBs mal formados).
// ─────────────────────────────────────────────────────────────────────────────
QString CasualModeController::extractBodyContent(const QString &fullHtml) {
  static const QRegularExpression kBody(
      QStringLiteral(R"(<body[^>]*>(.*?)</body>)"),
      QRegularExpression::DotMatchesEverythingOption |
          QRegularExpression::CaseInsensitiveOption);

  const auto match = kBody.match(fullHtml);
  if (match.hasMatch())
    return match.captured(1).trimmed();

  // Fallback: sem <body> — devolve o HTML completo (documentos EPUB simples)
  return fullHtml;
}

// ─────────────────────────────────────────────────────────────────────────────
// updateChapterTitle — procura no TOC o título mais próximo da posição actual.
//
// Para PDF  : usa o campo `page` das entradas (o maior page <= pageOrIndex).
// Para EPUB : compara o URL da spine com o URL da entrada de TOC.
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::updateChapterTitle(int pageOrIndex) {
  if (m_toc.isEmpty())
    return;

  if (!m_spineUrls.isEmpty()) {
    // EPUB — corresponde por URL da spine
    if (pageOrIndex < 0 || pageOrIndex >= m_spineUrls.size())
      return;
    const QString spineFile = m_spineUrls.at(pageOrIndex).fileName();

    for (const TocItem &t : m_toc) {
      if (t.url.fileName() == spineFile) {
        setChapterTitle(t.title);
        return;
      }
    }
    // Sem correspondência exacta — mantém o título anterior
  } else {
    // PDF — última entrada com page <= pageOrIndex
    QString best;
    for (const TocItem &t : m_toc) {
      if (t.page >= 0 && t.page <= pageOrIndex)
        best = t.title;
      else if (t.page > pageOrIndex)
        break;
    }
    if (!best.isEmpty())
      setChapterTitle(best);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers de estado simples
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setReadingProgress(qreal progress) {
  const qreal p = std::clamp(progress, 0.0, 1.0);
  if (qFuzzyCompare(m_readingProgress, p))
    return;
  m_readingProgress = p;
  emit readingProgressChanged();
}

void CasualModeController::setChapterTitle(const QString &title) {
  if (m_chapterTitle == title)
    return;
  m_chapterTitle = title;
  emit chapterTitleChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Cores derivadas do tema
// ─────────────────────────────────────────────────────────────────────────────
QString CasualModeController::bgColor() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].bg);
}
QString CasualModeController::textColor() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].text);
}
QString CasualModeController::headerBg() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].header);
}
QString CasualModeController::accentColor() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].accent);
}
QString CasualModeController::borderColor() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].border);
}
QString CasualModeController::mutedColor() const noexcept {
  return QString::fromLatin1(kPalettes[static_cast<int>(m_theme)].muted);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setters — tipografia, tema, painéis
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::setTheme(int theme) {
  const auto t = static_cast<Theme>(std::clamp(theme, 0, 3));
  if (m_theme == t)
    return;
  m_theme = t;
  emit themeChanged();
}

void CasualModeController::setFontSize(int size) {
  const int s = std::clamp(size, 10, 32);
  if (m_fontSize == s)
    return;
  m_fontSize = s;
  emit fontSizeChanged();
}

void CasualModeController::setColumnMargin(int margin) {
  const int m = std::clamp(margin, 24, 160);
  if (m_columnMargin == m)
    return;
  m_columnMargin = m;
  emit columnMarginChanged();
}

void CasualModeController::setLineSpacing(int spacing) {
  const int s = std::clamp(spacing, 0, 24);
  if (m_lineSpacing == s)
    return;
  m_lineSpacing = s;
  emit lineSpacingChanged();
}

void CasualModeController::setSidebarOpen(bool open) {
  if (m_sidebarOpen == open)
    return;
  m_sidebarOpen = open;
  emit sidebarOpenChanged();
}

void CasualModeController::setSearchOpen(bool open) {
  if (m_searchOpen == open)
    return;
  m_searchOpen = open;
  emit searchOpenChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Invokables — acções de UI
// ─────────────────────────────────────────────────────────────────────────────
void CasualModeController::increaseFontSize() { setFontSize(m_fontSize + 1); }
void CasualModeController::decreaseFontSize() { setFontSize(m_fontSize - 1); }
void CasualModeController::increaseMargin() {
  setColumnMargin(m_columnMargin + 8);
}
void CasualModeController::decreaseMargin() {
  setColumnMargin(m_columnMargin - 8);
}
void CasualModeController::toggleSidebar() { setSidebarOpen(!m_sidebarOpen); }
void CasualModeController::toggleSearch() { setSearchOpen(!m_searchOpen); }
