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
#include <QLineEdit>
#include "bookmark_item_widget.hpp"
#include "pdf_canvas_view.hpp"
#include "DocumentEngine/highlight_manager.hpp"
#include <QAction>
#include <QActionGroup>
#include <QVariantAnimation>
#include <QWidget>
#include <QFileSystemWatcher>
#include <QList>
#include <QUrl>
#include <QDBusArgument>
#include <QVariantMap>
#include <QCloseEvent>
#include <memory>

#include "CasualMode/casual_mode_widget.hpp"
#include "ViewLayer/casual_pdf_view.hpp"

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

enum class ViewIndex : int { Pdf = 0, Web = 1, Casual = 2, CasualPdf = 3 };

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
    void closeEvent(QCloseEvent* event) override;  // auto-salva no PDF (comportamento Okular)

private slots:
    void onModeChanged(ModeType newMode, ModeType previous);
    void onPageChanged(int page);
    void onZoomChanged(qreal zoom);
    // Disparado pelo debounce timer após o zoom parar de mudar.
    void onZoomSettled();
    void openFileDialog();
    void onPortalResponse(uint response, const QVariantMap &results);

    // ── EPUB slots ────────────────────────────────────────────────────────
    void onWebLoadFinished(bool ok);
    void onWebUrlChanged(const QUrl& url);

    // ── TOC slots ─────────────────────────────────────────────────────────
    void onTocReady(QList<TocEntry> entries);
    void onTocEmpty();
    void onSidePanelRequested(const QString& btnObjectName);

    // ── Bookmark slots ────────────────────────────────────────────────────
    void onAddBookmarkClicked();
    void onBookmarksChanged(const QList<BookmarkEntry>& bm);
    void onBookmarkPageStatusChanged(int page);
    void onHighlightRequested(const HighlightEntry& h);

    // ── Search slots ──────────────────────────────────────────────────────
    void runSearch();

    // ── Gallery slots ──────────────────────────────────────────────────────
    void populateGallery(int pageCount);
    void onGalleryPageReady(int page);
    void scheduleVisibleGalleryThumbs();
    void updateGalleryItemBadges(int page);

    // ── Kinetic scroll ─────────────────────────────────────────────────────
    void onScrollTick();

private:
    void autoSaveToPdf();
    [[nodiscard]] QPixmap paintBadges(const QPixmap& thumb, int page) const;

    // ── UI central ────────────────────────────────────────────────────────
    QStackedWidget*  m_stack      = nullptr;
    QScrollArea*     m_pdfScroll  = nullptr;
    PdfCanvasView*   m_pdfView    = nullptr;
    QWebEngineView*  m_webView    = nullptr;
    CasualModeWidget* m_casualWidget = nullptr;
    CasualPdfView*    m_casualPdfView = nullptr;  // spread de 2 páginas (PDF + Casual)

    // ── Toolbar principal (top) ───────────────────────────────────────────
    QToolBar*    m_toolbar        = nullptr;

    // ── Sidebar esquerda ──────────────────────────────────────────────────
    QWidget*         m_sidebar        = nullptr;
    QWidget*         m_sideIconBar    = nullptr;
    QStackedWidget*  m_sideStack      = nullptr;

    // ── Painel TOC (index 0 em m_sideStack) ──────────────────────────────
    QLabel*      m_sidebarTocLbl  = nullptr;
    QTreeWidget* m_tocTree        = nullptr;

    // ── Painel de Busca (index 1 em m_sideStack) ─────────────────────────
    QLineEdit*   m_searchInput   = nullptr;
    QListWidget* m_searchResults = nullptr;
    QLabel*      m_searchStatus  = nullptr;

    // ── Painel de Galeria (index 2 em m_sideStack) ───────────────────────
    QListWidget* m_galleryList  = nullptr;
    QLabel*      m_galleryEmpty = nullptr;

    // ── Painel de Marcadores (index 4 em m_sideStack) ────────────────────
    QListWidget* m_bookmarkList    = nullptr;
    QLabel*      m_bookmarkEmpty   = nullptr;
    QToolButton* m_addBookmarkBtn  = nullptr;

    // ── Botão ativo na sideIconBar ────────────────────────────────────────
    QToolButton* m_activeSideBtn  = nullptr;

    // ── Book info widget (Casual sidebar) ─────────────────────────────────
    QWidget*     m_bookInfoWidget = nullptr;
    QLabel*      m_bookCoverLabel = nullptr;
    QLabel*      m_bookTitleLabel = nullptr;
    QLabel*      m_bookAuthorLabel= nullptr;

    // ── Tab bar inferior da sidebar (Modo Casual) ─────────────────────────
    // Alterna entre TOC (0), Anotações (3) e Marcadores (4)
    QWidget*     m_casualTabBar   = nullptr;

    // ── Painel de anotações — Modo Estudo (dock direito) ──────────────────
    QDockWidget* m_annotDock      = nullptr;

    // ── Painel de Anotações (sidebar esquerdo, SidePanel::Edit) ──────────
    QListWidget* m_annotList      = nullptr;
    QLabel*      m_annotEmpty     = nullptr;

    // ── Managers de documentos ────────────────────────────────────────────
    std::unique_ptr<PdfBookmarkManager> m_bookmarkManager;
    std::unique_ptr<HighlightManager>   m_highlightManager;
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
    std::unique_ptr<TocWorker>           m_tocWorker;

    // ── EPUB navigation state ─────────────────────────────────────────────
    QList<QUrl>  m_spineUrls;
    int          m_currentChapter = 0;

    // ── Smooth scroll (navegação programática) ────────────────────────────
    QVariantAnimation* m_scrollAnim = nullptr;

    // ── Kinetic scroll (roda do mouse) ─────────────────────────────────────
    QTimer* m_scrollKineticTimer  = nullptr;
    qreal   m_scrollVelocity      = 0.0;

    // ── Zoom debounce ───────────────────────────────────────────────────────
    QTimer* m_zoomDebounceTimer = nullptr;
    qreal   m_pendingZoom       = 1.0;

    // ── Hot reload QSS ────────────────────────────────────────────────────
    QFileSystemWatcher* m_cssWatcher    = nullptr;
    QString             m_currentSsPath;

    // ── Helpers de construção (main_window_ui.cpp) ────────────────────────
    void buildUi();
    void buildActions();
    void buildMenuBar();
    void buildToolBarActions();
    void buildAnnotDock();
    void buildBottomNav();
    void buildChapterHeader();
    void buildStatusBar();

    // ── Helpers de runtime ────────────────────────────────────────────────
    void switchView(ViewIndex idx);
    void updatePageIndicator(int page, int total);
    void updateProgressLabel(int page, int total);

    // ── TOC helpers (main_window_toc.cpp) ─────────────────────────────────
    void populateTocTree(const QList<TocEntry>& entries);
    void highlightTocEntry(int page);
    void showSidePanel(SidePanel panel, bool forceOpen = true);

    // ── Bookmark helpers (main_window_bookmarks.cpp) ──────────────────────
    void populateBookmarkPanel(const QList<BookmarkEntry>& bookmarks);
    void updateAddBookmarkBtnState(int page);
    void saveBookmarksAsync();
    void addBookmarkItemWidget(const BookmarkEntry& bm);

    // ── EPUB helpers (main_window_epub.cpp) ───────────────────────────────
    void loadEpubChapter(int index);
    void injectEpubCSS();

    [[nodiscard]] QString notesDir() const;

    // ── Wire helpers — cada subsistema conecta os próprios sinais ─────────
    // Implementados no .cpp de cada subsistema; chamados por wireSignals().
    void wireSignals();
    void wireScrollSignals();
    void wirePageZoomSignals();
    void wireTocSignals();
    void wireBookmarkSignals();
    void wireHighlightSignals();
    void wireGallerySignals();
    void wireEpubSignals();
    void wireModeSignals();
    void wireDocumentSignals();
};
