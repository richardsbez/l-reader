// main_window_ui.cpp  —  l-reader
//
// Construção da interface: todos os métodos build*().
// Nenhuma lógica de negócio ou ligação de sinais aqui.

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"
#include "bookmark_item_widget.hpp"

#include "Ui/layout_tokens.hpp"
#include "Ui/ui_strings.hpp"

#include <QFrame>
#include <QProgressBar>
#include <QApplication>
#include <QActionGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QKeySequence>
#include <QButtonGroup>
#include <QWebEngineView>
#include <QWebEngineSettings>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// buildUi — monta toda a estrutura de widgets central + sidebar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildUi()
{
    m_toolbar = addToolBar(QStringLiteral("Principal"));
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(Layout::Toolbar::kIconSize);

    // ══════════════════════════════════════════════════════════════════════════
    // SIDEBAR ESQUERDA
    // ══════════════════════════════════════════════════════════════════════════
    auto* dock = new QDockWidget(QString::fromLatin1(UiStr::kSidebarDockTitle), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);

    auto* sideContainer = new QWidget(dock);
    sideContainer->setObjectName(QStringLiteral("sideContainer"));
    auto* sideLayout    = new QVBoxLayout(sideContainer);
    sideLayout->setContentsMargins(Layout::Structural::kNoMargins);
    sideLayout->setSpacing(Layout::Structural::kNoSpacing);

    // ── Book info widget (visível apenas no Modo Casual) ─────────────────
    // Inspirado na sidebar do Kindle/Thorium: capa compacta à esquerda,
    // título + autor à direita, altura total ~88px.
    //
    //  ┌────────────────────────────────────────┐
    //  │ ┌──────┐  Moby Dick                    │
    //  │ │ capa │  Herman Melville               │
    //  │ │      │  ──── p.12 / 342 ─── 3%       │
    //  │ └──────┘                                │
    //  └────────────────────────────────────────┘
    m_bookInfoWidget = new QWidget(sideContainer);
    m_bookInfoWidget->setObjectName(QStringLiteral("bookInfoWidget"));
    m_bookInfoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* bookRow = new QHBoxLayout(m_bookInfoWidget);
    bookRow->setContentsMargins(12, 10, 12, 10);
    bookRow->setSpacing(11);

    // ── Capa (56×76) ──────────────────────────────────────────────────
    m_bookCoverLabel = new QLabel(m_bookInfoWidget);
    m_bookCoverLabel->setObjectName(QStringLiteral("bookCoverLabel"));
    m_bookCoverLabel->setFixedSize(56, 76);
    m_bookCoverLabel->setAlignment(Qt::AlignCenter);
    m_bookCoverLabel->setScaledContents(true);
    m_bookCoverLabel->setText(QStringLiteral("📖"));
    bookRow->addWidget(m_bookCoverLabel, 0, Qt::AlignTop);

    // ── Info: título + autor + progresso ─────────────────────────────
    auto* infoCol = new QVBoxLayout;
    infoCol->setContentsMargins(0, 2, 0, 0);
    infoCol->setSpacing(2);

    m_bookTitleLabel = new QLabel(m_bookInfoWidget);
    m_bookTitleLabel->setObjectName(QStringLiteral("bookTitleLabel"));
    m_bookTitleLabel->setWordWrap(true);
    m_bookTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_bookTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_bookAuthorLabel = new QLabel(m_bookInfoWidget);
    m_bookAuthorLabel->setObjectName(QStringLiteral("bookAuthorLabel"));
    m_bookAuthorLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_bookAuthorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Linha de progresso inline: barra fina + "37%"
    auto* progressRow = new QHBoxLayout;
    progressRow->setContentsMargins(0, 4, 0, 0);
    progressRow->setSpacing(5);

    auto* casualProgressBar = new QProgressBar(m_bookInfoWidget);
    casualProgressBar->setObjectName(QStringLiteral("casualSideProgressBar"));
    casualProgressBar->setRange(0, 100);
    casualProgressBar->setValue(0);
    casualProgressBar->setTextVisible(false);
    casualProgressBar->setFixedHeight(3);
    casualProgressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* casualProgressPct = new QLabel(QStringLiteral("0%"), m_bookInfoWidget);
    casualProgressPct->setObjectName(QStringLiteral("casualSideProgressPct"));
    casualProgressPct->setFixedWidth(28);
    casualProgressPct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    progressRow->addWidget(casualProgressBar);
    progressRow->addWidget(casualProgressPct);

    infoCol->addWidget(m_bookTitleLabel);
    infoCol->addWidget(m_bookAuthorLabel);
    infoCol->addLayout(progressRow);
    infoCol->addStretch();

    bookRow->addLayout(infoCol, 1);
    m_bookInfoWidget->hide();

    // ── Barra de ícones (visível em Standard / Study) ─────────────────────
    m_sideIconBar = new QWidget(sideContainer);
    m_sideIconBar->setObjectName(QStringLiteral("sidebarIconBar"));
    auto* iconBarLayout = new QHBoxLayout(m_sideIconBar);
    iconBarLayout->setContentsMargins(Layout::SidebarIconBar::kMargins);
    iconBarLayout->setSpacing(Layout::SidebarIconBar::kSpacing);

    for (const auto& ic : UiStr::kSideIcons) {
        auto* btn = new QToolButton(m_sideIconBar);
        btn->setIcon(QIcon(QString::fromLatin1(ic.iconPath)));
        btn->setObjectName(QString::fromLatin1(ic.objectName));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setFixedSize(Layout::SidebarIconBar::kBtnSize);
        btn->setCheckable(true);
        iconBarLayout->addWidget(btn);

        const QString btnName = QString::fromLatin1(ic.objectName);
        connect(btn, &QToolButton::clicked, this, [this, btnName] {
            onSidePanelRequested(btnName);
        });
    }
    iconBarLayout->addStretch();

    // ── Stack de painéis de conteúdo ──────────────────────────────────────
    m_sideStack = new QStackedWidget(sideContainer);

    // ── Painel 0: TOC ─────────────────────────────────────────────────────
    {
        auto* tocPanel  = new QWidget(m_sideStack);
        auto* tocLayout = new QVBoxLayout(tocPanel);
        tocLayout->setContentsMargins(0, 0, 0, 0);
        tocLayout->setSpacing(0);

        m_sidebarTocLbl = new QLabel(QString::fromLatin1(UiStr::kSidebarTocLabel),
                                      tocPanel);
        m_sidebarTocLbl->setObjectName(QStringLiteral("sidebarTocLabel"));
        m_sidebarTocLbl->setAlignment(Qt::AlignCenter);

        m_tocTree = new QTreeWidget(tocPanel);
        m_tocTree->setHeaderHidden(true);
        m_tocTree->setRootIsDecorated(true);
        m_tocTree->setIndentation(Layout::TocTree::kIndentation);
        m_tocTree->setUniformRowHeights(true);

        tocLayout->addWidget(m_sidebarTocLbl);
        tocLayout->addWidget(m_tocTree, 1);
        m_sideStack->addWidget(tocPanel);           // index 0 → SidePanel::Toc
    }

    // ── Painel 1: Busca de texto ──────────────────────────────────────────
    {
        auto* searchPanel  = new QWidget(m_sideStack);
        searchPanel->setObjectName(QStringLiteral("searchPanel"));
        auto* searchLayout = new QVBoxLayout(searchPanel);
        searchLayout->setContentsMargins(0, 0, 0, 0);
        searchLayout->setSpacing(0);

        auto* searchHeader = new QLabel(tr("Buscar"), searchPanel);
        searchHeader->setObjectName(QStringLiteral("sidebarSearchLabel"));
        searchHeader->setAlignment(Qt::AlignCenter);

        auto* inputContainer = new QWidget(searchPanel);
        inputContainer->setObjectName(QStringLiteral("searchInputContainer"));
        auto* inputLayout = new QHBoxLayout(inputContainer);
        inputLayout->setContentsMargins(10, 8, 10, 8);
        inputLayout->setSpacing(0);

        m_searchInput = new QLineEdit(inputContainer);
        m_searchInput->setObjectName(QStringLiteral("searchInput"));
        m_searchInput->setPlaceholderText(tr("Pesquisar…"));
        m_searchInput->setClearButtonEnabled(true);
        inputLayout->addWidget(m_searchInput);

        m_searchStatus = new QLabel(searchPanel);
        m_searchStatus->setObjectName(QStringLiteral("searchStatus"));
        m_searchStatus->setAlignment(Qt::AlignCenter);
        m_searchStatus->setWordWrap(true);
        m_searchStatus->hide();

        m_searchResults = new QListWidget(searchPanel);
        m_searchResults->setObjectName(QStringLiteral("searchResultsList"));

        searchLayout->addWidget(searchHeader);
        searchLayout->addWidget(inputContainer);
        searchLayout->addWidget(m_searchStatus);
        searchLayout->addWidget(m_searchResults, 1);

        m_sideStack->addWidget(searchPanel);        // index 1 → SidePanel::Search

        connect(m_searchInput, &QLineEdit::returnPressed,
                this, &MainWindow::runSearch);

        connect(m_searchResults, &QListWidget::itemActivated, this,
                [this](QListWidgetItem* item) {
            if (!m_pdfView || !item) return;
            const int page = item->data(Qt::UserRole).toInt();
            m_pdfView->goToPage(page);
            const auto rects =
                item->data(Qt::UserRole + 1).value<QList<QRectF>>();
            if (!rects.isEmpty())
                m_pdfView->setSearchHighlights(page, rects);
        });
    }

    // ── Painel 2: Galeria ─────────────────────────────────────────────────
    {
        auto* galleryPanel  = new QWidget(m_sideStack);
        galleryPanel->setObjectName(QStringLiteral("galleryPanel"));
        auto* galleryLayout = new QVBoxLayout(galleryPanel);
        galleryLayout->setContentsMargins(0, 0, 0, 0);
        galleryLayout->setSpacing(0);

        auto* galleryHeader = new QLabel(tr("Galeria"), galleryPanel);
        galleryHeader->setObjectName(QStringLiteral("sidebarGalleryLabel"));
        galleryHeader->setAlignment(Qt::AlignCenter);

        m_galleryList = new QListWidget(galleryPanel);
        m_galleryList->setObjectName(QStringLiteral("galleryList"));
        m_galleryList->setViewMode(QListView::IconMode);
        m_galleryList->setFlow(QListView::TopToBottom);
        m_galleryList->setWrapping(false);
        m_galleryList->setResizeMode(QListView::Adjust);
        m_galleryList->setMovement(QListView::Static);
        m_galleryList->setSpacing(6);
        m_galleryList->setUniformItemSizes(true);
        m_galleryList->setIconSize(QSize(160, 226));
        m_galleryList->hide();

        m_galleryEmpty = new QLabel(
            tr("Abra um PDF para\nvisualizar as páginas\ncomo miniaturas."),
            galleryPanel);
        m_galleryEmpty->setObjectName(QStringLiteral("galleryEmptyLabel"));
        m_galleryEmpty->setAlignment(Qt::AlignCenter);
        m_galleryEmpty->setWordWrap(true);

        galleryLayout->addWidget(galleryHeader);
        galleryLayout->addWidget(m_galleryList,  1);
        galleryLayout->addWidget(m_galleryEmpty, 1);

        m_sideStack->addWidget(galleryPanel);       // index 2 → SidePanel::Gallery
    }

    // ── Painel 3: Anotações ────────────────────────────────────────────────
    {
        auto* annotPanel  = new QWidget(m_sideStack);
        annotPanel->setObjectName(QStringLiteral("annotSidePanel"));
        auto* annotLayout = new QVBoxLayout(annotPanel);
        annotLayout->setContentsMargins(0, 4, 0, 0);
        annotLayout->setSpacing(0);

        auto* annotHeader = new QLabel(tr("Anotações"), annotPanel);
        annotHeader->setObjectName(QStringLiteral("sidebarAnnotLabel"));
        annotHeader->setAlignment(Qt::AlignCenter);

        m_annotEmpty = new QLabel(
            tr("Nenhuma anotação.\nAtive ✏ e selecione texto."), annotPanel);
        m_annotEmpty->setObjectName(QStringLiteral("annotEmptyLabel"));
        m_annotEmpty->setAlignment(Qt::AlignCenter);
        m_annotEmpty->setWordWrap(true);

        m_annotList = new QListWidget(annotPanel);
        m_annotList->setObjectName(QStringLiteral("annotList"));
        m_annotList->setFrameStyle(QFrame::NoFrame);
        m_annotList->setSpacing(0);
        m_annotList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_annotList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_annotList->hide();

        annotLayout->addWidget(annotHeader);
        annotLayout->addWidget(m_annotEmpty, 1);
        annotLayout->addWidget(m_annotList, 1);

        m_sideStack->addWidget(annotPanel);         // index 3 → SidePanel::Edit
    }

    // ── Painel 4: Marcadores ──────────────────────────────────────────────
    {
        auto* bmPanel  = new QWidget(m_sideStack);
        bmPanel->setObjectName(QStringLiteral("bookmarkPanel"));
        auto* bmLayout = new QVBoxLayout(bmPanel);
        bmLayout->setContentsMargins(0, 4, 0, 0);
        bmLayout->setSpacing(0);

        auto* bmHeader = new QLabel(
            QString::fromUtf8(UiStr::kBookmarkPanelTitle), bmPanel);
        bmHeader->setObjectName(QStringLiteral("sidebarBookmarkLabel"));
        bmHeader->setAlignment(Qt::AlignCenter);

        m_bookmarkEmpty = new QLabel(
            QString::fromUtf8(UiStr::kBookmarkEmptyMsg), bmPanel);
        m_bookmarkEmpty->setObjectName(QStringLiteral("bookmarkEmptyLabel"));
        m_bookmarkEmpty->setAlignment(Qt::AlignCenter);
        m_bookmarkEmpty->setWordWrap(true);

        m_bookmarkList = new QListWidget(bmPanel);
        m_bookmarkList->setObjectName(QStringLiteral("bookmarkList"));
        m_bookmarkList->setFrameStyle(QFrame::NoFrame);
        m_bookmarkList->setSpacing(0);
        m_bookmarkList->setContentsMargins(0, 0, 0, 0);
        m_bookmarkList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_bookmarkList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_bookmarkList->setUniformItemSizes(false);
        m_bookmarkList->setSelectionMode(QAbstractItemView::SingleSelection);
        m_bookmarkList->hide();

        bmLayout->addWidget(bmHeader);
        bmLayout->addWidget(m_bookmarkEmpty, 1);
        bmLayout->addWidget(m_bookmarkList, 1);

        m_sideStack->addWidget(bmPanel);            // index 4 → SidePanel::Bookmarks
    }

    // ── Tab bar inferior — Modo Casual (estilo Kindle: Sumário | Notas | Marcadores)
    //    Visível apenas no Modo Casual; alterna os painéis do m_sideStack.
    m_casualTabBar = new QWidget(sideContainer);
    m_casualTabBar->setObjectName(QStringLiteral("casualTabBar"));
    m_casualTabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* tabBarLayout = new QHBoxLayout(m_casualTabBar);
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);

    struct CasualTab {
        const char* label;
        const char* objName;
        const char* iconPath;
        SidePanel   panel;
    };
    static constexpr CasualTab kCasualTabs[] = {
        { "Sumário",    "casualTabToc",       ":/icons/List.svg",     SidePanel::Toc       },
        { "Anotações",  "casualTabAnnot",     ":/icons/Edit.svg",     SidePanel::Edit      },
        { "Marcadores", "casualTabBookmarks", ":/icons/Bookmark.svg", SidePanel::Bookmarks },
    };

    for (const auto& tab : kCasualTabs) {
        auto* btn = new QToolButton(m_casualTabBar);
        btn->setText(tr(tab.label));
        btn->setIcon(QIcon(QString::fromLatin1(tab.iconPath)));
        btn->setObjectName(QString::fromLatin1(tab.objName));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setIconSize(QSize(18, 18));
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(44);

        const SidePanel panel = tab.panel;
        connect(btn, &QToolButton::clicked, this, [this, panel] {
            m_sideStack->setCurrentIndex(static_cast<int>(panel));
            // Sincroniza estado visual dos botões
            const auto btns = m_casualTabBar->findChildren<QToolButton*>();
            for (auto* b : btns)
                b->setChecked(b == sender());
        });
        tabBarLayout->addWidget(btn);
    }
    // Ativa "Sumário" por padrão
    if (auto* first = m_casualTabBar->findChild<QToolButton*>(
            QStringLiteral("casualTabToc")))
        first->setChecked(true);

    m_casualTabBar->hide();

    // ── Botão "Adicionar marcador" — fixado no fundo do sidebar ──────────
    m_addBookmarkBtn = new QToolButton(sideContainer);
    m_addBookmarkBtn->setObjectName(QString::fromLatin1(UiStr::kBookmarkAddBtnObjName));
    m_addBookmarkBtn->setText(QString::fromUtf8(UiStr::kBookmarkAddBtnText));
    m_addBookmarkBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addBookmarkBtn->setIcon(QIcon(QStringLiteral(":/icons/Bookmark.svg")));
    m_addBookmarkBtn->setCheckable(true);
    m_addBookmarkBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_addBookmarkBtn->setEnabled(false);

    sideLayout->addWidget(m_bookInfoWidget);
    sideLayout->addWidget(m_sideIconBar);
    sideLayout->addWidget(m_sideStack, 1);
    sideLayout->addWidget(m_casualTabBar);
    sideLayout->addWidget(m_addBookmarkBtn);
    dock->setWidget(sideContainer);

    addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->hide();
    m_sidebar = dock;

    // ══════════════════════════════════════════════════════════════════════════
    // ÁREA DE CONTEÚDO CENTRAL
    // ══════════════════════════════════════════════════════════════════════════
    m_pdfScroll = new QScrollArea(this);
    m_pdfScroll->setFrameStyle(QFrame::NoFrame);
    m_pdfScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_pdfScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_pdfScroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_pdfScroll->setWidgetResizable(true);

    m_pdfView = new PdfCanvasView(m_pdfScroll);
    m_pdfScroll->setWidget(m_pdfView);

    m_webView = new QWebEngineView(this);
    m_webView->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_webView->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);

    m_stack = new QStackedWidget(this);
    m_stack->insertWidget(static_cast<int>(ViewIndex::Pdf), m_pdfScroll);
    m_stack->insertWidget(static_cast<int>(ViewIndex::Web), m_webView);

    m_casualWidget = new CasualModeWidget(this);
    m_stack->insertWidget(static_cast<int>(ViewIndex::Casual), m_casualWidget);

    setCentralWidget(m_stack);
    switchView(ViewIndex::Pdf);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildAnnotDock — painel de anotações (Modo Estudo, dock direito)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildAnnotDock()
{
    auto* dock = new QDockWidget(QStringLiteral("Anotações"), this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setTitleBarWidget(new QWidget(dock));

    auto* panel = new QWidget(dock);
    panel->setObjectName(QStringLiteral("annotPanel"));
    panel->setFixedWidth(Layout::AnnotPanel::kWidth);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(Layout::AnnotPanel::kMargins);
    layout->setSpacing(Layout::AnnotPanel::kSpacing);

    for (const auto& t : UiStr::kAnnotTools) {
        auto* btn = new QToolButton(panel);
        btn->setIcon(QIcon(QString::fromUtf8(t.text)));
        btn->setObjectName(QString::fromLatin1(t.objectName));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setFixedSize(Layout::AnnotPanel::kBtnSize);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
    }

    layout->addStretch(1);

    auto* settingsBtn = new QToolButton(panel);
    settingsBtn->setIcon(QIcon(QString::fromUtf8(UiStr::kAnnotSettingsTool.text)));
    settingsBtn->setObjectName(QString::fromLatin1(UiStr::kAnnotSettingsTool.objectName));
    settingsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    settingsBtn->setFixedSize(Layout::AnnotPanel::kBtnSize);
    layout->addWidget(settingsBtn, 0, Qt::AlignHCenter);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    m_annotDock = dock;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildBottomNav — barra de navegação inferior (Modo Casual)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildBottomNav()
{
    m_bottomNav = new QToolBar(QStringLiteral("NavInferior"), this);
    m_bottomNav->setObjectName(QStringLiteral("bottomNavBar"));
    m_bottomNav->setMovable(false);
    m_bottomNav->setIconSize(QSize(16, 16));

    addToolBar(Qt::BottomToolBarArea, m_bottomNav);

    m_actCasualPrev = new QAction(QString::fromUtf8(UiStr::kCasualPrev), this);
    m_actCasualNext = new QAction(QString::fromUtf8(UiStr::kCasualNext), this);

    m_bottomNav->addAction(m_actCasualPrev);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_bottomNav->widgetForAction(m_actCasualPrev))) {
        btn->setObjectName(QStringLiteral("casualPrevBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(Layout::BottomNav::kNavBtn);
    }

    m_progressLabel = new QLabel(QStringLiteral("0%"), this);
    m_progressLabel->setObjectName(QStringLiteral("progressLabel"));
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setFixedWidth(40);
    m_bottomNav->addWidget(m_progressLabel);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setObjectName(QStringLiteral("progressSlider"));
    m_progressSlider->setRange(0, 100);
    m_progressSlider->setValue(0);
    m_progressSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bottomNav->addWidget(m_progressSlider);

    m_bottomNav->addAction(m_actCasualNext);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_bottomNav->widgetForAction(m_actCasualNext))) {
        btn->setObjectName(QStringLiteral("casualNextBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(Layout::BottomNav::kNavBtn);
    }

    m_bottomNav->hide();
}

// ─────────────────────────────────────────────────────────────────────────────
// buildChapterHeader — faixa de capítulo (Modo Casual, topo do conteúdo)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildChapterHeader()
{
    auto* hdrToolbar = new QToolBar(QStringLiteral("CabeçalhoCapítulo"), this);
    hdrToolbar->setObjectName(QStringLiteral("chapterHeaderBar"));
    hdrToolbar->setMovable(false);

    addToolBar(Qt::TopToolBarArea, hdrToolbar);
    insertToolBarBreak(hdrToolbar);

    auto* leftLabel = new QLabel(QStringLiteral(""), hdrToolbar);
    leftLabel->setObjectName(QStringLiteral("chapterLabelLeft"));
    leftLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    leftLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* rightLabel = new QLabel(QStringLiteral(""), hdrToolbar);
    rightLabel->setObjectName(QStringLiteral("chapterLabelRight"));
    rightLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    rightLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hdrToolbar->addWidget(leftLabel);
    hdrToolbar->addWidget(rightLabel);

    hdrToolbar->hide();
    m_chapterHeader = hdrToolbar;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildActions — cria todas as QActions sem adicioná-las a nenhum widget
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildActions()
{
    m_actOpen = new QAction(
        style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Abrir…"), this);
    m_actOpen->setShortcut(QKeySequence::Open);
    m_actOpen->setStatusTip(tr("Abrir documento (PDF · EPUB · Markdown)"));

    m_actPrevPage = new QAction(this);
    m_actPrevPage->setIcon(QIcon(QString::fromLatin1(UiStr::kPrevPage)));
    m_actPrevPage->setStatusTip(tr("Página anterior  [← / PgUp]"));
    m_actPrevPage->setEnabled(false);

    m_actNextPage = new QAction(this);
    m_actNextPage->setIcon(QIcon(QString::fromLatin1(UiStr::kNextPage)));
    m_actNextPage->setStatusTip(tr("Próxima página  [→ / PgDn]"));
    m_actNextPage->setEnabled(false);

    m_actZoomIn = new QAction(this);
    m_actZoomIn->setIcon(QIcon(QString::fromLatin1(UiStr::kZoomIn)));
    m_actZoomIn->setShortcut(QKeySequence::ZoomIn);
    m_actZoomIn->setEnabled(false);

    m_actZoomOut = new QAction(this);
    m_actZoomOut->setIcon(QIcon(QString::fromLatin1(UiStr::kZoomOut)));
    m_actZoomOut->setShortcut(QKeySequence::ZoomOut);
    m_actZoomOut->setEnabled(false);

    m_actZoomReset = new QAction(tr("Zoom 100%"), this);
    m_actZoomReset->setShortcut(Qt::CTRL | Qt::Key_0);
    m_actZoomReset->setEnabled(false);

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);

    m_actModeStd = new QAction(tr("Padrão"), this);
    m_actModeStd->setCheckable(true);
    m_actModeStd->setChecked(true);
    modeGroup->addAction(m_actModeStd);

    m_actModeStudy = new QAction(tr("Estudo"), this);
    m_actModeStudy->setCheckable(true);
    modeGroup->addAction(m_actModeStudy);

    m_actModeCasual = new QAction(tr("Casual"), this);
    m_actModeCasual->setCheckable(true);
    modeGroup->addAction(m_actModeCasual);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildMenuBar — oculta a barra nativa; cria actQuit para o hamburguer
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildMenuBar()
{
    menuBar()->hide();

    auto* actQuit = new QAction(tr("&Sair"), this);
    actQuit->setObjectName(QStringLiteral("actQuit"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildToolBarActions — popula m_toolbar com botões e ações
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildToolBarActions()
{
    const auto makeIconBtn = [this](const char* iconPath,
                                    const char* objName,
                                    QSize       size) -> QToolButton*
    {
        auto* btn = new QToolButton(this);
        btn->setIcon(QIcon(QString::fromLatin1(iconPath)));
        btn->setObjectName(QString::fromLatin1(objName));
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setFixedSize(size);
        return btn;
    };

    // ── Grupo esquerdo ────────────────────────────────────────────────────
    auto* sideToggle = new QToolButton(this);
    sideToggle->setIcon(QIcon(QString::fromLatin1(UiStr::kSidebarToggle)));
    sideToggle->setObjectName(QStringLiteral("sidebarToggleBtn"));
    sideToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);
    sideToggle->setFixedSize(Layout::Toolbar::kSideToggleSize);

    m_toolbar->addWidget(sideToggle);
    m_toolbar->addSeparator();

    // Zoom
    m_toolbar->addAction(m_actZoomOut);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_toolbar->widgetForAction(m_actZoomOut))) {
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setObjectName(QStringLiteral("zoomOutBtn"));
        btn->setFixedSize(Layout::Toolbar::kZoomBtnSize);
    }

    m_zoomLabel = new QLabel(QString::fromLatin1(UiStr::kZoomDefault), this);
    m_zoomLabel->setObjectName(QStringLiteral("zoomLabel"));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setFixedWidth(Layout::Toolbar::kZoomLabelWidth);
    m_toolbar->addWidget(m_zoomLabel);

    m_toolbar->addAction(m_actZoomIn);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_toolbar->widgetForAction(m_actZoomIn))) {
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setObjectName(QStringLiteral("zoomInBtn"));
        btn->setFixedSize(Layout::Toolbar::kZoomBtnSize);
    }

    // View Mode
    auto* viewModeBtn = new QToolButton(this);
    viewModeBtn->setIcon(QIcon(QString::fromLatin1(UiStr::kViewModeIcon)));
    viewModeBtn->setText(QStringLiteral("  ") + QString::fromLatin1(UiStr::kViewModeText));
    viewModeBtn->setObjectName(QStringLiteral("viewModeBtn"));
    viewModeBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    viewModeBtn->setFixedHeight(Layout::Toolbar::kSideToggleSize.height());

    m_toolbar->addSeparator();
    m_toolbar->addWidget(viewModeBtn);

    // ── Expansores (centralizam a navegação) ──────────────────────────────
    const auto makeExpander = [this]() {
        auto* w = new QWidget(this);
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        return w;
    };
    m_toolbar->addWidget(makeExpander());

    // ── Grupo central — Navegação de página ───────────────────────────────
    m_toolbar->addAction(m_actPrevPage);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_toolbar->widgetForAction(m_actPrevPage))) {
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setObjectName(QStringLiteral("prevPageBtn"));
        btn->setFixedSize(Layout::Toolbar::kNavBtnSize);
    }

    m_pageLabel = new QLabel(QString::fromLatin1(UiStr::kPageDefault), this);
    m_pageLabel->setObjectName(QStringLiteral("pageLabel"));
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setFixedWidth(Layout::Toolbar::kPageLabelWidth);
    m_toolbar->addWidget(m_pageLabel);

    m_toolbar->addAction(m_actNextPage);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_toolbar->widgetForAction(m_actNextPage))) {
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setObjectName(QStringLiteral("nextPageBtn"));
        btn->setFixedSize(Layout::Toolbar::kNavBtnSize);
    }

    m_toolbar->addWidget(makeExpander());
    m_toolbar->addSeparator();

    // ── Grupo direito — Ferramentas UI ────────────────────────────────────
    for (const auto& t : UiStr::kUiTools) {
        m_toolbar->addWidget(
            makeIconBtn(t.iconPath, t.objectName, Layout::Toolbar::kToolBtnSize));
    }
    m_toolbar->addSeparator();

    // ── Dropdown de modos ─────────────────────────────────────────────────
    m_modeDropdown = new QToolButton(this);
    m_modeDropdown->setObjectName(QStringLiteral("modeDropdownBtn"));
    m_modeDropdown->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_modeDropdown->setPopupMode(QToolButton::InstantPopup);
    m_modeDropdown->setFixedSize(Layout::Toolbar::kModeDropdownSize);
    m_modeDropdown->setText(QString::fromUtf8(UiStr::kModeStd));

    auto* modeMenu = new QMenu(m_modeDropdown);
    modeMenu->setObjectName(QStringLiteral("modeMenu"));
    modeMenu->addAction(m_actModeStd);
    modeMenu->addAction(m_actModeStudy);
    modeMenu->addAction(m_actModeCasual);
    m_modeDropdown->setMenu(modeMenu);

    m_toolbar->addWidget(m_modeDropdown);

    // ── Hamburguer ────────────────────────────────────────────────────────
    {
        auto* hamburger = new QToolButton(this);
        hamburger->setText(QString::fromUtf8(UiStr::kHamburger));
        hamburger->setObjectName(QStringLiteral("hamburgerBtn"));
        hamburger->setToolButtonStyle(Qt::ToolButtonTextOnly);
        hamburger->setFixedSize(Layout::Toolbar::kHamburgerSize);
        hamburger->setPopupMode(QToolButton::InstantPopup);

        auto* rootMenu = new QMenu(hamburger);
        rootMenu->setObjectName(QStringLiteral("hamburgerMenu"));

        QMenu* fileMenu = rootMenu->addMenu(tr("Arquivo"));
        fileMenu->setObjectName(QStringLiteral("hamburgerFileMenu"));
        fileMenu->addAction(m_actOpen);
        fileMenu->addSeparator();
        if (auto* aq = findChild<QAction*>(QStringLiteral("actQuit")))
            fileMenu->addAction(aq);

        QMenu* viewMenu = rootMenu->addMenu(tr("Visualizar"));
        viewMenu->setObjectName(QStringLiteral("hamburgerViewMenu"));
        viewMenu->addAction(m_actZoomIn);
        viewMenu->addAction(m_actZoomOut);
        viewMenu->addAction(m_actZoomReset);
        viewMenu->addSeparator();
        viewMenu->addAction(m_actModeStd);
        viewMenu->addAction(m_actModeStudy);
        viewMenu->addAction(m_actModeCasual);

        hamburger->setMenu(rootMenu);
        m_toolbar->addWidget(hamburger);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildStatusBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildStatusBar()
{
    m_sbFile = new QLabel(tr("Nenhum documento aberto"), this);
    m_sbFile->setObjectName(QStringLiteral("sbFile"));
    m_sbFile->setContentsMargins(Layout::StatusBar::kFileLabelMargins);
    statusBar()->addWidget(m_sbFile, 1);

    m_sbMode = new QLabel(tr("Padrão"), this);
    m_sbMode->setObjectName(QStringLiteral("sbMode"));
    m_sbMode->setContentsMargins(Layout::StatusBar::kModeLabelMargins);
    statusBar()->addPermanentWidget(m_sbMode);
}
