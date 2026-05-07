// main_window.hpp
#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QWebEngineView>
#include <QToolBar>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QTreeWidget>
#include <QListWidget>
#include "bookmark_item_widget.hpp"
#include <QAction>
#include <QActionGroup>
#include <QVariantAnimation>
#include <QWidget>
#include <QFileSystemWatcher>
#include <QList>
#include <QUrl>
#include <QDBusArgument>
#include <QVariantMap>
#include <memory>

#include "DocumentEngine/document_engine.hpp"
#include "DocumentEngine/toc_worker.hpp"
#include "DocumentEngine/pdf_bookmark_manager.hpp"
#include "ModeManager/mode_manager.hpp"
#include "Persistence/sidecar_manager.hpp"
#include "RenderSubsystem/page_cache.hpp"

// ─── Estruturas D-Bus (FileChooser portal) ────────────────────────────────────
struct PortalFilterRule { uint type; QString pattern; };
Q_DECLARE_METATYPE(PortalFilterRule)
struct PortalFilter { QString name; QList<PortalFilterRule> rules; };
Q_DECLARE_METATYPE(PortalFilter)

QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilterRule &rule);
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilterRule &rule);
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilter &filter);
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilter &filter);

class PdfCanvasView;

enum class ViewIndex : int { Pdf = 0, Web = 1 };

// ─── Índice de painéis no m_sideStack ────────────────────────────────────────
enum class SidePanel : int {
    Toc       = 0,   // sideBtnList  — sumário
    Search    = 1,   // sideBtnSearch
    Gallery   = 2,   // sideBtnGallery
    Edit      = 3,   // sideBtnEdit
    Bookmarks = 4,   // sideBtnBookmark
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openDocument(const QString& path);

    // ── API usada pelos ReadingMode ───────────────────────────────────────
    void      applySS(const QString& resourcePath);
    QWidget*  sidebar()       const { return m_sidebar; }
    QDockWidget* annotationBar() const { return m_annotDock; }
    QToolBar* toolbar()       const { return m_toolbar; }

    QDockWidget* annotDock()     const { return m_annotDock; }
    QToolBar*    bottomNav()     const { return m_bottomNav; }
    QWidget*     chapterHeader() const { return m_chapterHeader; }

    void setMargins(const QMargins& m);
    void setSidebarCasualMode(bool casual);
    void smoothScrollTo(int targetY);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onModeChanged(ModeType newMode, ModeType previous);
    void onPageChanged(int page);
    void onZoomChanged(qreal zoom);
    void openFileDialog();
    void onPortalResponse(uint response, const QVariantMap &results);

    // ── EPUB slots ────────────────────────────────────────────────────────
    void onWebLoadFinished(bool ok);
    void onWebUrlChanged(const QUrl& url);

    // ── TOC slots ─────────────────────────────────────────────────────────
    // Recebe o resultado assíncrono do TocWorker e popula m_tocTree.
    void onTocReady(QList<TocEntry> entries);
    // Chamado quando o TocWorker confirma que o documento não tem sumário.
    void onTocEmpty();
    // Disparado por qualquer botão da sideIconBar; name = objectName do btn.
    void onSidePanelRequested(const QString& btnObjectName);

    // ── Bookmark slots ────────────────────────────────────────────────────
    void onAddBookmarkClicked();                               // toggle marcador na página atual
    void onBookmarksChanged(const QList<BookmarkEntry>& bm);  // atualiza painel lateral
    void onBookmarkSaveToPdfRequested();                       // embute no PDF
    void onBookmarkPageStatusChanged(int page);                // atualiza ícone preenchido/vazio

private:
    // ── UI central ────────────────────────────────────────────────────────
    QStackedWidget*  m_stack      = nullptr;
    QScrollArea*     m_pdfScroll  = nullptr;
    PdfCanvasView*   m_pdfView    = nullptr;
    QWebEngineView*  m_webView    = nullptr;

    // ── Toolbar principal (top) ───────────────────────────────────────────
    QToolBar*    m_toolbar        = nullptr;

    // ── Sidebar esquerda ──────────────────────────────────────────────────
    QWidget*         m_sidebar        = nullptr;   // QDockWidget
    QWidget*         m_sideIconBar    = nullptr;   // barra de ícones
    QStackedWidget*  m_sideStack      = nullptr;   // painéis de conteúdo

    // ── Painel TOC (index 0 em m_sideStack) ──────────────────────────────
    QLabel*      m_sidebarTocLbl  = nullptr;   // rótulo "Sumário" / "Carregando…"
    QTreeWidget* m_tocTree        = nullptr;   // árvore do sumário

    // ── Painel de Marcadores (index 4 em m_sideStack) ────────────────────
    QListWidget* m_bookmarkList    = nullptr;   // lista de entradas
    QLabel*      m_bookmarkEmpty   = nullptr;   // placeholder "nenhum marcador"
    QToolButton* m_addBookmarkBtn  = nullptr;   // botão fixado no fundo do sidebar

    // ── Botão ativo na sideIconBar (controle de toggle visual) ───────────
    QToolButton* m_activeSideBtn  = nullptr;

    // ── Book info widget (Casual sidebar) ─────────────────────────────────
    QWidget*     m_bookInfoWidget = nullptr;
    QLabel*      m_bookCoverLabel = nullptr;
    QLabel*      m_bookTitleLabel = nullptr;
    QLabel*      m_bookAuthorLabel= nullptr;

    // ── Painel de anotações — Modo Estudo (dock direito) ──────────────────
    QDockWidget* m_annotDock      = nullptr;
    QWidget*     m_annotCanvas    = nullptr;

    // ── Barra de navegação inferior — Modo Casual ─────────────────────────
    QToolBar*    m_bottomNav      = nullptr;
    QSlider*     m_progressSlider = nullptr;
    QLabel*      m_progressLabel  = nullptr;

    // ── Cabeçalho de capítulo — Modo Casual ──────────────────────────────
    QWidget*     m_chapterHeader  = nullptr;

    // ── Labels inline da toolbar ──────────────────────────────────────────
    QLabel* m_pageLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;

    // ── Ações ─────────────────────────────────────────────────────────────
    QAction* m_actOpen       = nullptr;
    QAction* m_actPrevPage   = nullptr;
    QAction* m_actNextPage   = nullptr;
    QAction* m_actZoomIn     = nullptr;
    QAction* m_actZoomOut    = nullptr;
    QAction* m_actZoomReset  = nullptr;
    QAction* m_actModeStd    = nullptr;
    QAction* m_actModeStudy  = nullptr;
    QAction* m_actModeCasual = nullptr;

    QToolButton* m_modeDropdown = nullptr;

    QAction* m_actCasualPrev = nullptr;
    QAction* m_actCasualNext = nullptr;

    // ── Status bar ────────────────────────────────────────────────────────
    QLabel* m_sbFile = nullptr;
    QLabel* m_sbMode = nullptr;

    // ── Subsistemas ───────────────────────────────────────────────────────
    std::unique_ptr<DocumentEngine>      m_engine;
    std::unique_ptr<ModeManager>         m_modeManager;
    std::unique_ptr<PageCache>           m_pageCache;
    std::unique_ptr<SidecarManager>      m_sidecar;
    std::unique_ptr<TocWorker>           m_tocWorker;       // worker assíncrono de TOC
    std::unique_ptr<PdfBookmarkManager>  m_bookmarkManager; // marcadores portáteis PDF

    // ── EPUB navigation state ─────────────────────────────────────────────
    QList<QUrl>  m_spineUrls;
    int          m_currentChapter = 0;

    // ── Smooth scroll ─────────────────────────────────────────────────────
    QVariantAnimation* m_scrollAnim = nullptr;

    // ── Hot reload QSS ────────────────────────────────────────────────────
    QFileSystemWatcher* m_cssWatcher    = nullptr;
    QString             m_currentSsPath;

    // ── Helpers de construção ─────────────────────────────────────────────
    void buildUi();
    void buildActions();
    void buildMenuBar();
    void buildToolBarActions();
    void buildAnnotDock();
    void buildBottomNav();
    void buildChapterHeader();
    void buildStatusBar();
    void wireSignals();

    // ── Helpers de runtime ────────────────────────────────────────────────
    void switchView(ViewIndex idx);
    void updatePageIndicator(int page, int total);
    void updateProgressLabel(int page, int total);

    // ── TOC helpers ───────────────────────────────────────────────────────
    void populateTocTree(const QList<TocEntry>& entries);
    void highlightTocEntry(int page);   // sincroniza seleção ao mudar de página (PDF)
    void showSidePanel(SidePanel panel, bool forceOpen = true);

    // ── Bookmark helpers ──────────────────────────────────────────────────
    void populateBookmarkPanel(const QList<BookmarkEntry>& bookmarks);
    void updateAddBookmarkBtnState(int page);  // ícone solid/outline p/ página atual
    void saveBookmarksAsync();                 // JSON sidecar (rápido)
    void addBookmarkItemWidget(const BookmarkEntry& bm); // adiciona 1 item à lista

    // ── EPUB helpers ──────────────────────────────────────────────────────
    void loadEpubChapter(int index);
    void injectEpubCSS();

    [[nodiscard]] QString notesDir() const;
};
