// main_window.cpp  —  l-reader
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  CAMADA DE LÓGICA — não contém literais de apresentação.                │
// │                                                                         │
// │  Para alterar a aparência da aplicação edite apenas:                    │
// │    • src/Ui/layout_tokens.hpp  — dimensões, margens e espaçamentos      │
// │    • src/Ui/ui_strings.hpp     — símbolos, rótulos e placeholders       │
// │    • src/Ui/epub_style.hpp     — CSS do leitor EPUB                     │
// │    • src/styles/*.qss          — cores, fontes e border-radius          │
// └─────────────────────────────────────────────────────────────────────────┘

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"
#include "DocumentEngine/pdf_engine.hpp"
#include "DocumentEngine/ebook_engine.hpp"
#include "DocumentEngine/toc_worker.hpp"
#include "DocumentEngine/pdf_bookmark_manager.hpp"
#include "bookmark_item_widget.hpp"

#include "Ui/layout_tokens.hpp"
#include "Ui/ui_strings.hpp"
#include "Ui/epub_style.hpp"

#include <QApplication>
#include <QActionGroup>
#include <QDockWidget>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QWheelEvent>
#include <QDir>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QUrl>
#include <QDebug>
#include <QWebEngineSettings>

using namespace Qt::Literals::StringLiterals;

static constexpr int kScrollAnimMs  = 320;
static constexpr int kScrollPxNotch = 120;

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    buildActions();
    buildMenuBar();
    buildToolBarActions();
    buildAnnotDock();
    buildBottomNav();
    buildChapterHeader();
    buildStatusBar();

    m_modeManager    = std::make_unique<ModeManager>(this, this);
    m_pageCache      = std::make_unique<PageCache>(this);
    m_sidecar        = std::make_unique<SidecarManager>(notesDir(), this);
    m_tocWorker      = std::make_unique<TocWorker>(this);
    m_bookmarkManager = std::make_unique<PdfBookmarkManager>(this);

    wireSignals();
}

// ─────────────────────────────────────────────────────────────────────────────
// buildUi
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
    auto* sideLayout    = new QVBoxLayout(sideContainer);
    sideLayout->setContentsMargins(Layout::Structural::kNoMargins);
    sideLayout->setSpacing(Layout::Structural::kNoSpacing);

    // ── Book info widget (visível apenas no Modo Casual) ─────────────────
    m_bookInfoWidget = new QWidget(sideContainer);
    m_bookInfoWidget->setObjectName(QStringLiteral("bookInfoWidget"));
    m_bookInfoWidget->setFixedHeight(Layout::BookInfo::kHeight);
    auto* bookLayout = new QVBoxLayout(m_bookInfoWidget);
    bookLayout->setContentsMargins(12, 12, 12, 8);
    bookLayout->setSpacing(4);

    // Capa
    m_bookCoverLabel = new QLabel(QString::fromLatin1(UiStr::kBookCoverPlaceholder),
                                  m_bookInfoWidget);
    m_bookCoverLabel->setObjectName(QStringLiteral("bookCoverLabel"));
    m_bookCoverLabel->setFixedSize(Layout::BookInfo::kCoverSize);
    m_bookCoverLabel->setAlignment(Qt::AlignCenter);

    // Título
    m_bookTitleLabel = new QLabel(QString::fromLatin1(UiStr::kBookTitlePlaceholder),
                                  m_bookInfoWidget);
    m_bookTitleLabel->setObjectName(QStringLiteral("bookTitleLabel"));
    m_bookTitleLabel->setWordWrap(true);

    // Autor
    m_bookAuthorLabel = new QLabel(QString::fromLatin1(UiStr::kBookAuthorPlaceholder),
                                   m_bookInfoWidget);
    m_bookAuthorLabel->setObjectName(QStringLiteral("bookAuthorLabel"));

    auto* coverRow = new QHBoxLayout;
    coverRow->addWidget(m_bookCoverLabel);
    coverRow->addStretch();
    bookLayout->addLayout(coverRow);
    bookLayout->addWidget(m_bookTitleLabel);
    bookLayout->addWidget(m_bookAuthorLabel);
    bookLayout->addStretch();
    m_bookInfoWidget->hide();   // escondido por padrão

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
        btn->setCheckable(true);    // feedback visual de painel activo
        iconBarLayout->addWidget(btn);

        // ── Wire: cada botão emite onSidePanelRequested com o seu nome ───
        const QString btnName = QString::fromLatin1(ic.objectName);
        connect(btn, &QToolButton::clicked, this, [this, btnName] {
            onSidePanelRequested(btnName);
        });
    }
    iconBarLayout->addStretch();

    // ── Stack de painéis de conteúdo ──────────────────────────────────────
    // Índices definidos em SidePanel (main_window.hpp).
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

    // ── Painéis 1–3: placeholders (Search, Gallery, Edit) ────────────────
    const std::array<const char*, 3> kPanelLabels {
        "🔍  Pesquisar", "🖼  Galeria", "✏  Anotações"
    };
    for (const char* lbl : kPanelLabels) {
        auto* ph  = new QLabel(QString::fromUtf8(lbl), m_sideStack);
        ph->setAlignment(Qt::AlignCenter);
        ph->setWordWrap(true);
        m_sideStack->addWidget(ph);
    }

    // ── Painel 4: Marcadores — real ───────────────────────────────────────
    {
        auto* bmPanel  = new QWidget(m_sideStack);
        bmPanel->setObjectName(QStringLiteral("bookmarkPanel"));
        auto* bmLayout = new QVBoxLayout(bmPanel);
        bmLayout->setContentsMargins(0, 4, 0, 0);
        bmLayout->setSpacing(0);

        // Cabeçalho
        auto* bmHeader = new QLabel(
            QString::fromUtf8(UiStr::kBookmarkPanelTitle), bmPanel);
        bmHeader->setObjectName(QStringLiteral("sidebarBookmarkLabel"));
        bmHeader->setAlignment(Qt::AlignCenter);

        // Placeholder quando vazio
        m_bookmarkEmpty = new QLabel(
            QString::fromUtf8(UiStr::kBookmarkEmptyMsg), bmPanel);
        m_bookmarkEmpty->setObjectName(QStringLiteral("bookmarkEmptyLabel"));
        m_bookmarkEmpty->setAlignment(Qt::AlignCenter);
        m_bookmarkEmpty->setWordWrap(true);

        // Lista de marcadores com widgets customizados
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

        // Botão "Embutir no PDF" — aparece só quando há marcadores
        auto* embedBtn = new QToolButton(bmPanel);
        embedBtn->setObjectName(QStringLiteral("bookmarkEmbedBtn"));
        embedBtn->setText(QString::fromUtf8(UiStr::kBookmarkEmbedBtnText));
        embedBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        embedBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        embedBtn->setToolTip(tr("Embute os marcadores como anotações portáteis no arquivo PDF."));
        embedBtn->hide();
        connect(embedBtn, &QToolButton::clicked,
                this, &MainWindow::onBookmarkSaveToPdfRequested);

        bmLayout->addWidget(bmHeader);
        bmLayout->addWidget(m_bookmarkEmpty, 1);
        bmLayout->addWidget(m_bookmarkList, 1);
        bmLayout->addWidget(embedBtn);

        m_sideStack->addWidget(bmPanel);   // index 4 → SidePanel::Bookmarks
    }

    // ── Botão "Adicionar marcador" — fixado no fundo do sidebar ──────────
    // Fica FORA do m_sideStack; sempre visível quando o sidebar está aberto.
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
    sideLayout->addWidget(m_addBookmarkBtn);   // ← fundo do sidebar
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
    setCentralWidget(m_stack);
    switchView(ViewIndex::Pdf);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildAnnotDock — Painel de anotações (Modo Estudo, dock direito)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildAnnotDock()
{
    auto* dock = new QDockWidget(QStringLiteral("Anotações"), this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setTitleBarWidget(new QWidget(dock));   // sem barra de título

    auto* panel = new QWidget(dock);
    panel->setObjectName(QStringLiteral("annotPanel"));
    panel->setFixedWidth(Layout::AnnotPanel::kWidth);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(Layout::AnnotPanel::kMargins);
    layout->setSpacing(Layout::AnnotPanel::kSpacing);

    // Ferramentas de anotação
    for (const auto& t : UiStr::kAnnotTools) {
        auto* btn = new QToolButton(panel);
        btn->setText(QString::fromUtf8(t.text));
        btn->setObjectName(QString::fromLatin1(t.objectName));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(Layout::AnnotPanel::kBtnSize);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
    }

    layout->addStretch(1);

    // Botão de configurações (na base)
    auto* settingsBtn = new QToolButton(panel);
    settingsBtn->setText(QString::fromUtf8(UiStr::kAnnotSettingsTool.text));
    settingsBtn->setObjectName(QString::fromLatin1(UiStr::kAnnotSettingsTool.objectName));
    settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    settingsBtn->setFixedSize(Layout::AnnotPanel::kBtnSize);
    layout->addWidget(settingsBtn, 0, Qt::AlignHCenter);

    dock->setWidget(panel);

    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
    m_annotDock = dock;
}

// ─────────────────────────────────────────────────────────────────────────────
// buildBottomNav — Barra de navegação inferior (Modo Casual)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildBottomNav()
{
    m_bottomNav = new QToolBar(QStringLiteral("NavInferior"), this);
    m_bottomNav->setObjectName(QStringLiteral("bottomNavBar"));
    m_bottomNav->setMovable(false);
    m_bottomNav->setIconSize(QSize(16, 16));

    addToolBar(Qt::BottomToolBarArea, m_bottomNav);

    // Placeholder — ações serão ligadas em wireSignals()
    m_actCasualPrev = new QAction(QString::fromUtf8(UiStr::kCasualPrev), this);
    m_actCasualNext = new QAction(QString::fromUtf8(UiStr::kCasualNext), this);

    m_bottomNav->addAction(m_actCasualPrev);
    if (auto* btn = qobject_cast<QToolButton*>(
            m_bottomNav->widgetForAction(m_actCasualPrev))) {
        btn->setObjectName(QStringLiteral("casualPrevBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setFixedSize(Layout::BottomNav::kNavBtn);
    }

    // Rótulo de percentagem
    m_progressLabel = new QLabel(QStringLiteral("0%"), this);
    m_progressLabel->setObjectName(QStringLiteral("progressLabel"));
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setFixedWidth(40);
    m_bottomNav->addWidget(m_progressLabel);

    // Slider de progresso
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
// buildChapterHeader — Faixa de capítulo (Modo Casual, topo do conteúdo)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildChapterHeader()
{
    // Implementado como um widget flutuante sobre o conteúdo.
    // É posicionado como toolbar no topo (abaixo da toolbar principal).
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
// buildActions
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
// buildMenuBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildMenuBar()
{
    // Barra de menus nativa ocultada: as opcoes ficam no hamburguer da toolbar.
    menuBar()->hide();

    // actQuit criado aqui com filho da janela; recuperado em buildToolBarActions.
    auto* actQuit = new QAction(tr("&Sair"), this);
    actQuit->setObjectName(QStringLiteral("actQuit"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildToolBarActions
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
    // Um único QToolButton exibe o modo ativo; as demais opções aparecem
    // apenas ao clicar, num QMenu suspenso — sem ocupar espaço permanente.
    m_modeDropdown = new QToolButton(this);
    m_modeDropdown->setObjectName(QStringLiteral("modeDropdownBtn"));
    m_modeDropdown->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_modeDropdown->setPopupMode(QToolButton::InstantPopup);
    m_modeDropdown->setFixedSize(Layout::Toolbar::kModeDropdownSize);
    // Texto inicial: modo padrão + seta visual
    m_modeDropdown->setText(QString::fromUtf8(UiStr::kModeStd));

    auto* modeMenu = new QMenu(m_modeDropdown);
    modeMenu->setObjectName(QStringLiteral("modeMenu"));
    modeMenu->addAction(m_actModeStd);
    modeMenu->addAction(m_actModeStudy);
    modeMenu->addAction(m_actModeCasual);
    m_modeDropdown->setMenu(modeMenu);

    m_toolbar->addWidget(m_modeDropdown);

    // ── Hamburguer — abre QMenu com Arquivo / Visualizar ─────────────────
    {
        auto* hamburger = new QToolButton(this);
        hamburger->setText(QString::fromUtf8(UiStr::kHamburger));
        hamburger->setObjectName(QStringLiteral("hamburgerBtn"));
        hamburger->setToolButtonStyle(Qt::ToolButtonTextOnly);
        hamburger->setFixedSize(Layout::Toolbar::kHamburgerSize);
        hamburger->setPopupMode(QToolButton::InstantPopup);

        auto* rootMenu = new QMenu(hamburger);
        rootMenu->setObjectName(QStringLiteral("hamburgerMenu"));

        // ── Arquivo ──────────────────────────────────────────────────────
        QMenu* fileMenu = rootMenu->addMenu(tr("Arquivo"));
        fileMenu->setObjectName(QStringLiteral("hamburgerFileMenu"));
        fileMenu->addAction(m_actOpen);
        fileMenu->addSeparator();
        // actQuit foi criado em buildMenuBar() como filho da janela
        if (auto* aq = findChild<QAction*>(QStringLiteral("actQuit")))
            fileMenu->addAction(aq);

        // ── Visualizar ───────────────────────────────────────────────────
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
    m_sbFile->setContentsMargins(Layout::StatusBar::kFileLabelMargins);
    statusBar()->addWidget(m_sbFile, 1);

    m_sbMode = new QLabel(tr("Padrão"), this);
    m_sbMode->setContentsMargins(Layout::StatusBar::kModeLabelMargins);
    statusBar()->addPermanentWidget(m_sbMode);
}

// ─────────────────────────────────────────────────────────────────────────────
// wireSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireSignals()
{
    connect(m_modeManager.get(), &ModeManager::modeChanged,
            this, &MainWindow::onModeChanged);

    connect(m_pageCache.get(), &PageCache::pageReady,
            m_pdfView, &PdfCanvasView::requestRepaintPage);
    connect(m_pdfView, &PdfCanvasView::currentPageChanged,
            m_pageCache.get(), &PageCache::onCurrentPageChanged);
    connect(m_pdfView, &PdfCanvasView::currentPageChanged,
            this, &MainWindow::onPageChanged);
    connect(m_pdfView, &PdfCanvasView::zoomChanged,
            this, &MainWindow::onZoomChanged);
    connect(m_pdfScroll->verticalScrollBar(), &QScrollBar::valueChanged,
            m_pdfView, &PdfCanvasView::onScrollChanged);

    // Smooth scroll
    m_scrollAnim = new QVariantAnimation(this);
    m_scrollAnim->setDuration(kScrollAnimMs);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scrollAnim, &QVariantAnimation::valueChanged,
            this, [this](const QVariant& v) {
        m_pdfScroll->verticalScrollBar()->setValue(v.toInt());
    });

    m_pdfView->installEventFilter(this);
    m_pdfScroll->viewport()->installEventFilter(this);

    connect(m_actOpen, &QAction::triggered, this, &MainWindow::openFileDialog);

    // ── Sidebar toggle ────────────────────────────────────────────────────
    if (auto* btn = findChild<QToolButton*>(QStringLiteral("sidebarToggleBtn")))
        connect(btn, &QToolButton::clicked, this, [this] {
            m_sidebar->setVisible(!m_sidebar->isVisible());
        });

    connect(m_actPrevPage, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == static_cast<int>(ViewIndex::Web))
            loadEpubChapter(m_currentChapter - 1);
        else
            m_pdfView->goToPage(m_pdfView->currentPage() - 1);
    });
    connect(m_actNextPage, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == static_cast<int>(ViewIndex::Web))
            loadEpubChapter(m_currentChapter + 1);
        else
            m_pdfView->goToPage(m_pdfView->currentPage() + 1);
    });

    // Casual bottom-nav (espelha prev/next page)
    connect(m_actCasualPrev, &QAction::triggered, this, [this] {
        m_actPrevPage->trigger();
    });
    connect(m_actCasualNext, &QAction::triggered, this, [this] {
        m_actNextPage->trigger();
    });

    // Progresso slider (placeholder — altera de página proporcionalmente)
    connect(m_progressSlider, &QSlider::valueChanged, this, [this](int val) {
        if (!m_progressLabel) return;
        m_progressLabel->setText(QStringLiteral("%1%").arg(val));
    });

    connect(m_actZoomIn, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == static_cast<int>(ViewIndex::Web)) {
            const qreal nz = std::min(m_webView->zoomFactor() + 0.1, 3.0);
            m_webView->setZoomFactor(nz);
            onZoomChanged(nz);
        } else {
            m_pdfView->zoomIn();
        }
    });
    connect(m_actZoomOut, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == static_cast<int>(ViewIndex::Web)) {
            const qreal nz = std::max(m_webView->zoomFactor() - 0.1, 0.3);
            m_webView->setZoomFactor(nz);
            onZoomChanged(nz);
        } else {
            m_pdfView->zoomOut();
        }
    });
    connect(m_actZoomReset, &QAction::triggered, this, [this] {
        if (m_stack->currentIndex() == static_cast<int>(ViewIndex::Web)) {
            m_webView->setZoomFactor(1.0);
            onZoomChanged(1.0);
        } else {
            m_pdfView->zoomReset();
        }
    });

    connect(m_actModeStd,    &QAction::triggered, this,
            [this]{ m_modeManager->transitionTo(ModeType::Standard); });
    connect(m_actModeStudy,  &QAction::triggered, this,
            [this]{ m_modeManager->transitionTo(ModeType::Study); });
    connect(m_actModeCasual, &QAction::triggered, this,
            [this]{ m_modeManager->transitionTo(ModeType::Casual); });

    connect(m_webView, &QWebEngineView::loadFinished,
            this, &MainWindow::onWebLoadFinished);
    connect(m_webView, &QWebEngineView::urlChanged,
            this, &MainWindow::onWebUrlChanged);

    connect(m_tocTree, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem* item, int) {
        if (!item || !m_engine) return;

        // ── PDF: navega para a página armazenada em Qt::UserRole (int) ───
        if (m_engine->type() == DocumentType::PDF) {
            const int page = item->data(0, Qt::UserRole).toInt();
            if (page >= 0)
                m_pdfView->goToPage(page);
            return;
        }

        // ── EPUB: navega para a URL (já existente) ────────────────────────
        const QUrl url = item->data(0, Qt::UserRole).toUrl();
        if (!url.isValid()) return;

        QUrl urlBase = url;
        urlBase.setFragment({});
        for (int i = 0; i < m_spineUrls.size(); ++i) {
            QUrl spineBase = m_spineUrls[i];
            spineBase.setFragment({});
            if (spineBase == urlBase) {
                loadEpubChapter(i);
                const QString frag = url.fragment();
                if (!frag.isEmpty())
                    m_webView->setProperty("_pendingAnchor", frag);
                return;
            }
        }
        m_webView->load(url);
    });

    // ── TocWorker — resultados assíncronos ────────────────────────────────
    connect(m_tocWorker.get(), &TocWorker::tocReady,
            this, &MainWindow::onTocReady);
    connect(m_tocWorker.get(), &TocWorker::tocEmpty,
            this, &MainWindow::onTocEmpty);

    // ── PdfBookmarkManager ────────────────────────────────────────────────
    connect(m_bookmarkManager.get(), &PdfBookmarkManager::bookmarksChanged,
            this, &MainWindow::onBookmarksChanged);
    connect(m_bookmarkManager.get(), &PdfBookmarkManager::saved,
            this, [this](bool ok, const QString& err) {
        if (!ok)
            statusBar()->showMessage(
                QString::fromUtf8(UiStr::kBookmarkSaveError)
                + QLatin1Char(' ') + err, 5000);
    });
    connect(m_bookmarkManager.get(), &PdfBookmarkManager::pdfSaved,
            this, [this](bool ok, const QString& err) {
        if (ok)
            statusBar()->showMessage(tr("Marcadores embutidos no PDF."), 3000);
        else
            statusBar()->showMessage(
                tr("Erro ao embutir: ") + err, 6000);
        // Re-habilita o botão após operação
        if (auto* btn = findChild<QToolButton*>(
                QStringLiteral("bookmarkEmbedBtn")))
            btn->setEnabled(true);
    });

    // ── Botão "Adicionar marcador" ─────────────────────────────────────────
    connect(m_addBookmarkBtn, &QToolButton::clicked,
            this, &MainWindow::onAddBookmarkClicked);

    // ── Contexto no m_bookmarkList — botão direito para remover ───────────
    m_bookmarkList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bookmarkList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        if (!m_bookmarkList || !m_bookmarkManager) return;
        auto* item = m_bookmarkList->itemAt(pos);
        if (!item) return;

        // Recupera o widget customizado do item
        auto* w = qobject_cast<BookmarkItemWidget*>(
            m_bookmarkList->itemWidget(item));
        if (!w) return;

        QMenu menu(m_bookmarkList);
        auto* actNav    = menu.addAction(tr("Ir para página %1").arg(w->page() + 1));
        auto* actRename = menu.addAction(tr("Renomear"));
        menu.addSeparator();
        auto* actDel    = menu.addAction(tr("Remover marcador"));

        const QAction* chosen = menu.exec(
            m_bookmarkList->viewport()->mapToGlobal(pos));

        if (chosen == actNav)
            m_pdfView->goToPage(w->page());
        else if (chosen == actRename)
            w->startEditing();
        else if (chosen == actDel) {
            m_bookmarkManager->removeBookmark(w->page());
            saveBookmarksAsync();
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// setSidebarCasualMode — alterna o conteúdo do sidebar entre modos
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setSidebarCasualMode(bool casual)
{
    if (m_bookInfoWidget)  m_bookInfoWidget->setVisible(casual);
    if (m_sideIconBar)     m_sideIconBar->setVisible(!casual);
    if (m_sidebarTocLbl)   m_sidebarTocLbl->setVisible(!casual);

    if (m_tocTree) {
        m_tocTree->setRootIsDecorated(!casual);
        m_tocTree->setProperty("casualMode", casual);
        m_tocTree->style()->unpolish(m_tocTree);
        m_tocTree->style()->polish(m_tocTree);
    }

    // Em modo Casual o painel de conteúdo não é mostrado; em outros modos
    // garantimos que o stack está visível.
    if (m_sideStack)
        m_sideStack->setVisible(!casual);
}

// ─────────────────────────────────────────────────────────────────────────────
// eventFilter — smooth scroll + Ctrl+roda=zoom
// ─────────────────────────────────────────────────────────────────────────────
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    const bool isScrollTarget = (obj == m_pdfView ||
                                 obj == m_pdfScroll->viewport());
    if (!isScrollTarget || event->type() != QEvent::Wheel)
        return QMainWindow::eventFilter(obj, event);

    auto* we = static_cast<QWheelEvent*>(event);

    if (we->modifiers() & Qt::ControlModifier) {
        if (we->angleDelta().y() > 0) m_pdfView->zoomIn();
        else                           m_pdfView->zoomOut();
        return true;
    }

    const int delta = -we->angleDelta().y();
    const int step  = (delta * kScrollPxNotch) / 120;
    QScrollBar* vsb = m_pdfScroll->verticalScrollBar();

    const int currentTarget = (m_scrollAnim->state() == QAbstractAnimation::Running)
        ? m_scrollAnim->endValue().toInt()
        : vsb->value();

    const int newTarget = std::clamp(currentTarget + step,
                                     vsb->minimum(), vsb->maximum());

    if (m_scrollAnim->state() == QAbstractAnimation::Running)
        m_scrollAnim->stop();

    m_scrollAnim->setStartValue(vsb->value());
    m_scrollAnim->setEndValue(newTarget);
    m_scrollAnim->start();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// smoothScrollTo
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::smoothScrollTo(int targetY)
{
    QScrollBar* vsb = m_pdfScroll->verticalScrollBar();
    const int clamped = std::clamp(targetY, vsb->minimum(), vsb->maximum());
    if (m_scrollAnim->state() == QAbstractAnimation::Running)
        m_scrollAnim->stop();
    m_scrollAnim->setStartValue(vsb->value());
    m_scrollAnim->setEndValue(clamped);
    m_scrollAnim->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// D-Bus serialization
// ─────────────────────────────────────────────────────────────────────────────
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilterRule &rule) {
    arg.beginStructure(); arg << rule.type << rule.pattern; arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilterRule &rule) {
    arg.beginStructure(); arg >> rule.type >> rule.pattern; arg.endStructure();
    return arg;
}
QDBusArgument &operator<<(QDBusArgument &arg, const PortalFilter &filter) {
    arg.beginStructure(); arg << filter.name << filter.rules; arg.endStructure();
    return arg;
}
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalFilter &filter) {
    arg.beginStructure(); arg >> filter.name >> filter.rules; arg.endStructure();
    return arg;
}

// ─────────────────────────────────────────────────────────────────────────────
// openFileDialog
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openFileDialog()
{
    qDBusRegisterMetaType<PortalFilterRule>();
    qDBusRegisterMetaType<QList<PortalFilterRule>>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<QList<PortalFilter>>();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        u"org.freedesktop.portal.Desktop"_s,
        u"/org/freedesktop/portal/desktop"_s,
        u"org.freedesktop.portal.FileChooser"_s,
        u"OpenFile"_s);

    QVariantMap options;
    options[u"multiple"_s] = false;
    QList<PortalFilter> filters = {
        {tr("Documentos"),
            {{0, u"*.pdf"_s},{0, u"*.epub"_s},{0, u"*.mobi"_s},
             {0, u"*.azw"_s},{0, u"*.md"_s},{0, u"*.markdown"_s}}},
        {tr("PDF (*.pdf)"),  {{0, u"*.pdf"_s}}},
        {tr("EPUB (*.epub)"),{{0, u"*.epub"_s}}},
        {tr("MOBI (*.mobi)"),{{0, u"*.mobi"_s},{0, u"*.azw"_s}}},
        {tr("Markdown"),     {{0, u"*.md"_s},{0, u"*.markdown"_s}}},
        {tr("Todos (*)"),    {{0, u"*"_s}}}
    };
    options[u"filters"_s] = QVariant::fromValue(filters);
    msg << u""_s << tr("Abrir documento") << options;

    auto* watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);

    connect(watcher, &QDBusPendingCallWatcher::finished,
            this, [this](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qWarning() << "Portal D-Bus error:" << reply.error().message();
            return;
        }
        QDBusConnection::sessionBus().connect(
            u"org.freedesktop.portal.Desktop"_s,
            reply.value().path(),
            u"org.freedesktop.portal.Request"_s,
            u"Response"_s,
            this, SLOT(onPortalResponse(uint, QVariantMap)));
    });
}

void MainWindow::onPortalResponse(uint response, const QVariantMap &results)
{
    if (response == 0 && results.contains(u"uris"_s)) {
        const QStringList uris = results[u"uris"_s].toStringList();
        if (!uris.isEmpty())
            openDocument(QUrl(uris.first()).toLocalFile());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// openDocument
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openDocument(const QString& path)
{
    m_engine = DocumentEngine::create(path, this);

    if (!m_engine) {
        setWindowTitle(QStringLiteral("l-reader — formato não suportado"));
        m_sbFile->setText(
            tr("Formato não suportado: %1").arg(QFileInfo(path).fileName()));
        return;
    }

    connect(m_engine.get(), &DocumentEngine::documentLoaded,
            this, [this](const QString& p, int pages)
    {
        const QString fileName = QFileInfo(p).fileName();
        setWindowTitle(QStringLiteral("l-reader — %1").arg(fileName));

        if (m_engine->type() == DocumentType::PDF) {
            auto* pdf = qobject_cast<PDFEngine*>(m_engine.get());
            m_pageCache->setEngine(pdf->rawDocument(), pages);
            m_pdfView->setDocument(m_pageCache.get(), pdf->rawDocument(), pages);
            m_pdfScroll->verticalScrollBar()->setValue(0);
            switchView(ViewIndex::Pdf);
            m_pdfView->setFocus();
            m_sbFile->setText(fileName);
            updatePageIndicator(0, pages);
            m_actPrevPage->setEnabled(false);
            m_actNextPage->setEnabled(pages > 1);
            m_actZoomIn->setEnabled(true);
            m_actZoomOut->setEnabled(true);
            m_actZoomReset->setEnabled(true);
            m_zoomLabel->setText(QString::fromLatin1(UiStr::kZoomDefault));

            // Update book info for casual mode
            if (m_bookTitleLabel)  m_bookTitleLabel->setText(fileName);
            if (m_bookAuthorLabel) m_bookAuthorLabel->setText(QString());

        } else {
            auto* ebook = qobject_cast<EBookEngine*>(m_engine.get());
            if (!ebook || ebook->spineUrls().isEmpty()) {
                m_sbFile->setText(tr("Erro: EPUB sem capítulos"));
                return;
            }
            m_spineUrls      = ebook->spineUrls();
            m_currentChapter = 0;
            m_webView->setZoomFactor(1.0);
            switchView(ViewIndex::Web);
            const QString title = ebook->title().isEmpty() ? fileName : ebook->title();
            m_sbFile->setText(title);
            updatePageIndicator(0, m_spineUrls.size());
            m_actPrevPage->setEnabled(false);
            m_actNextPage->setEnabled(m_spineUrls.size() > 1);
            m_actZoomIn->setEnabled(true);
            m_actZoomOut->setEnabled(true);
            m_actZoomReset->setEnabled(true);
            m_zoomLabel->setText(QString::fromLatin1(UiStr::kZoomDefault));
            loadEpubChapter(0);

            // Update book info for casual mode
            if (m_bookTitleLabel)  m_bookTitleLabel->setText(title);
            if (m_bookAuthorLabel) m_bookAuthorLabel->setText(ebook->title());
        }

        // ── Extração de TOC assíncrona (PDF e EPUB) ───────────────────────
        // Mostra "Carregando…" no rótulo enquanto o worker não responde.
        if (m_sidebarTocLbl)
            m_sidebarTocLbl->setText(tr("Carregando sumário…"));
        if (m_tocTree)
            m_tocTree->clear();
        m_tocWorker->extract(m_engine.get());

        // ── Inicializa marcadores (somente PDF) ────────────────────────────
        if (m_engine->type() == DocumentType::PDF) {
            auto* pdf = qobject_cast<PDFEngine*>(m_engine.get());
            m_bookmarkManager->setDocument(pdf->rawDocument(), p, notesDir());
            if (m_addBookmarkBtn) m_addBookmarkBtn->setEnabled(true);
            updateAddBookmarkBtnState(0);
        } else {
            m_bookmarkManager->clear();
            if (m_addBookmarkBtn) m_addBookmarkBtn->setEnabled(false);
            populateBookmarkPanel({});
        }

        m_sidecar->trackDocument(p);
        m_modeManager->transitionTo(ModeType::Standard);
    });

    connect(m_engine.get(), &DocumentEngine::loadFailed,
            this, [this](const QString& error) {
        setWindowTitle(QStringLiteral("l-reader — erro ao abrir"));
        m_sbFile->setText(tr("Erro ao abrir documento"));
        qWarning() << "[l-reader] loadFailed:" << error;
    });

    m_engine->load(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadEpubChapter
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::loadEpubChapter(int index)
{
    if (m_spineUrls.isEmpty()) return;
    index = std::clamp(index, 0, static_cast<int>(m_spineUrls.size()) - 1);
    m_currentChapter = index;

    m_webView->load(m_spineUrls[index]);

    updatePageIndicator(index, m_spineUrls.size());
    m_actPrevPage->setEnabled(index > 0);
    m_actNextPage->setEnabled(index < m_spineUrls.size() - 1);
}

void MainWindow::onWebLoadFinished(bool ok)
{
    if (!ok) return;
    if (m_stack->currentIndex() != static_cast<int>(ViewIndex::Web)) return;

    injectEpubCSS();

    const QString anchor = m_webView->property("_pendingAnchor").toString();
    if (!anchor.isEmpty()) {
        m_webView->setProperty("_pendingAnchor", QVariant());
        const QString js = QStringLiteral(
            "var el = document.getElementById('%1') || "
            "document.querySelector('[name=\"%1\"]');"
            "if(el) el.scrollIntoView({behavior:'smooth'});")
            .arg(anchor);
        m_webView->page()->runJavaScript(js);
    }
}

void MainWindow::onWebUrlChanged(const QUrl& url)
{
    if (m_spineUrls.isEmpty()) return;

    QUrl urlBase = url;
    urlBase.setFragment({});

    for (int i = 0; i < m_spineUrls.size(); ++i) {
        QUrl spineBase = m_spineUrls[i];
        spineBase.setFragment({});

        if (spineBase == urlBase && i != m_currentChapter) {
            m_currentChapter = i;
            updatePageIndicator(i, m_spineUrls.size());
            m_actPrevPage->setEnabled(i > 0);
            m_actNextPage->setEnabled(i < m_spineUrls.size() - 1);
            m_sidecar->savePagePosition(i);
            return;
        }
    }
}

void MainWindow::injectEpubCSS()
{
    const QString escaped = QString::fromLatin1(EpubStyle::kReadingCSS)
        .replace(QLatin1Char('`'), QLatin1String("\\`"));

    const QString js = QStringLiteral(R"JS(
        (function() {
            var old = document.getElementById('lreader-css');
            if (old) old.remove();
            var s = document.createElement('style');
            s.id = 'lreader-css';
            s.textContent = `%1`;
            document.head.appendChild(s);
        })();
    )JS").arg(escaped);

    m_webView->page()->runJavaScript(js);
}

// ─────────────────────────────────────────────────────────────────────────────
// populateTocTree — chamado por onTocReady com a lista já extraída
//
// Para PDF : entry.page >= 0, entry.url inválida → Qt::UserRole armazena int
// Para EPUB: entry.url válida, entry.page == -1  → Qt::UserRole armazena QUrl
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::populateTocTree(const QList<TocEntry>& entries)
{
    if (!m_tocTree) return;
    m_tocTree->clear();

    if (entries.isEmpty()) {
        // Fallback EPUB: usa spine diretamente
        for (int i = 0; i < m_spineUrls.size(); ++i) {
            auto* item = new QTreeWidgetItem(m_tocTree);
            item->setText(0, tr("Capítulo %1").arg(i + 1));
            item->setData(0, Qt::UserRole, m_spineUrls[i]);
        }
    } else {
        QVector<QTreeWidgetItem*> stack;
        for (const TocEntry& entry : entries) {
            auto* item = new QTreeWidgetItem();
            item->setText(0, entry.title.isEmpty()
                              ? tr("(sem título)") : entry.title);
            item->setToolTip(0, entry.title);

            // Armazena o dado de navegação adequado ao tipo
            if (entry.page >= 0)
                item->setData(0, Qt::UserRole, entry.page);
            else
                item->setData(0, Qt::UserRole, entry.url);

            const int depth = std::max(0, entry.depth);
            while (stack.size() > depth)
                stack.removeLast();

            if (stack.isEmpty())
                m_tocTree->addTopLevelItem(item);
            else
                stack.last()->addChild(item);

            stack.append(item);
        }
    }

    m_tocTree->expandAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// highlightTocEntry — sincroniza a seleção na árvore ao mudar de página (PDF)
//
// Seleciona o último item cujo page <= currentPage (comportamento de TOC
// "rolling" — como nos leitores de eBook).
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::highlightTocEntry(int page)
{
    if (!m_tocTree || !m_engine) return;
    if (m_engine->type() != DocumentType::PDF) return;

    QTreeWidgetItem* best     = nullptr;
    int              bestPage = -1;

    // Busca DFS sobre toda a árvore
    std::function<void(QTreeWidgetItem*)> dfs = [&](QTreeWidgetItem* it) {
        const int itemPage = it->data(0, Qt::UserRole).toInt();
        if (itemPage >= 0 && itemPage <= page && itemPage > bestPage) {
            best     = it;
            bestPage = itemPage;
        }
        for (int c = 0; c < it->childCount(); ++c)
            dfs(it->child(c));
    };
    for (int i = 0; i < m_tocTree->topLevelItemCount(); ++i)
        dfs(m_tocTree->topLevelItem(i));

    if (best) {
        // Bloqueia o sinal para não disparar navegação ao mudar a seleção
        m_tocTree->blockSignals(true);
        m_tocTree->setCurrentItem(best);
        m_tocTree->scrollToItem(best, QAbstractItemView::EnsureVisible);
        m_tocTree->blockSignals(false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// showSidePanel — abre o sidebar e muda para o painel indicado.
//                 forceOpen=false permite fechar se já estiver nesse painel.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::showSidePanel(SidePanel panel, bool forceOpen)
{
    const int idx = static_cast<int>(panel);
    auto* dock    = qobject_cast<QDockWidget*>(m_sidebar);
    if (!dock) return;

    if (!forceOpen && dock->isVisible() && m_sideStack->currentIndex() == idx) {
        dock->hide();   // toggle: fecha se já está neste painel
        return;
    }

    m_sideStack->setCurrentIndex(idx);
    dock->show();
    dock->raise();
}

// ─────────────────────────────────────────────────────────────────────────────
// onSidePanelRequested — disparado pelos botões da sideIconBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onSidePanelRequested(const QString& btnObjectName)
{
    // Mapeia objectName → SidePanel
    static const QHash<QString, SidePanel> kMap = {
        { QStringLiteral("sideBtnList"),     SidePanel::Toc       },
        { QStringLiteral("sideBtnSearch"),   SidePanel::Search    },
        { QStringLiteral("sideBtnGallery"),  SidePanel::Gallery   },
        { QStringLiteral("sideBtnEdit"),     SidePanel::Edit      },
        { QStringLiteral("sideBtnBookmark"), SidePanel::Bookmarks },
    };

    const auto it = kMap.find(btnObjectName);
    if (it == kMap.end()) return;

    // Toggle: clicar no mesmo painel aberto fecha o sidebar
    showSidePanel(it.value(), /*forceOpen=*/false);

    // Atualiza estado visual dos botões (checked = painel activo)
    const auto buttons = m_sideIconBar->findChildren<QToolButton*>();
    for (auto* btn : buttons)
        btn->setChecked(btn->objectName() == btnObjectName
                        && qobject_cast<QDockWidget*>(m_sidebar)->isVisible());
}

// ─────────────────────────────────────────────────────────────────────────────
// onTocReady — slot chamado pelo TocWorker quando a extração assíncrona termina
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onTocReady(QList<TocEntry> entries)
{
    if (m_sidebarTocLbl)
        m_sidebarTocLbl->setText(tr("Sumário"));

    populateTocTree(entries);
}

// ─────────────────────────────────────────────────────────────────────────────
// onTocEmpty — documento sem sumário
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onTocEmpty()
{
    if (m_sidebarTocLbl)
        m_sidebarTocLbl->setText(tr("Sumário indisponível"));

    populateTocTree({});   // fallback de spine para EPUB, vazio para PDF
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots de runtime
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onPageChanged(int page)
{
    const int total = m_pdfView->pageCount();
    updatePageIndicator(page, total);
    m_actPrevPage->setEnabled(page > 0);
    m_actNextPage->setEnabled(page < total - 1);
    highlightTocEntry(page);          // sincroniza seleção no sumário PDF
    onBookmarkPageStatusChanged(page); // atualiza botão ＋ (preenchido/vazio)
}

void MainWindow::onZoomChanged(qreal zoom)
{
    m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
}

void MainWindow::updatePageIndicator(int page, int total)
{
    if (total > 0) {
        m_pageLabel->setText(
            QStringLiteral("%1 / %2").arg(page + 1).arg(total));
        updateProgressLabel(page, total);
    } else {
        m_pageLabel->setText(QString::fromLatin1(UiStr::kPageDefault));
    }
}

void MainWindow::updateProgressLabel(int page, int total)
{
    if (!m_progressSlider || total <= 0) return;
    const int pct = static_cast<int>(
        (static_cast<double>(page) / (total - 1)) * 100.0);
    m_progressSlider->setValue(pct);
    if (m_progressLabel)
        m_progressLabel->setText(QStringLiteral("%1%").arg(pct));
}

void MainWindow::switchView(ViewIndex idx)
{
    m_stack->setCurrentIndex(static_cast<int>(idx));
}

void MainWindow::onModeChanged(ModeType newMode, ModeType /*previous*/)
{
    m_actModeStd->setChecked(newMode    == ModeType::Standard);
    m_actModeStudy->setChecked(newMode  == ModeType::Study);
    m_actModeCasual->setChecked(newMode == ModeType::Casual);

    // Atualiza o rótulo visível do dropdown com o nome do modo ativo
    if (m_modeDropdown && m_modeManager)
        m_modeDropdown->setText(m_modeManager->currentName());

    if (m_sbMode && m_modeManager)
        m_sbMode->setText(m_modeManager->currentName());
}

// ─────────────────────────────────────────────────────────────────────────────
// applySS (hot-reload QSS)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::applySS(const QString& resourcePath)
{
    if (!m_cssWatcher) {
        m_cssWatcher = new QFileSystemWatcher(this);
        connect(m_cssWatcher, &QFileSystemWatcher::fileChanged,
                this, [this](const QString& changedPath) {
            if (!m_cssWatcher->files().contains(changedPath))
                m_cssWatcher->addPath(changedPath);
            QFile f(changedPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                setStyleSheet(QString::fromUtf8(f.readAll()));
        });
    }

    const QString relPath = QString(resourcePath).remove(0, 2);
    const QString execDir = QCoreApplication::applicationDirPath();
    QString diskPath;
    for (const QString& c : QStringList{
            execDir + u'/' + relPath,
            execDir + QStringLiteral("/styles/") + QFileInfo(relPath).fileName()
         })
    {
        if (QFile::exists(c)) { diskPath = c; break; }
    }

    if (!m_currentSsPath.isEmpty())
        m_cssWatcher->removePath(m_currentSsPath);

    if (!diskPath.isEmpty()) {
        QFile f(diskPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(f.readAll()));
        m_cssWatcher->addPath(diskPath);
        m_currentSsPath = diskPath;
    } else {
        QFile f(resourcePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            setStyleSheet(QString::fromUtf8(f.readAll()));
        m_currentSsPath.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setMargins / notesDir
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setMargins(const QMargins& m)
{
    if (m_pdfScroll) m_pdfScroll->setContentsMargins(m);
    if (m_webView)   m_webView->setContentsMargins(m);
}

QString MainWindow::notesDir() const
{
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::AppDataLocation)
                        + QStringLiteral("/notes");
    QDir().mkpath(dir);
    return dir;
}

// ─────────────────────────────────────────────────────────────────────────────
// populateBookmarkPanel — reconstrói a lista com BookmarkItemWidget por item
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::populateBookmarkPanel(const QList<BookmarkEntry>& bookmarks)
{
    if (!m_bookmarkList || !m_bookmarkEmpty) return;

    m_bookmarkList->clear();

    // Controla visibilidade do placeholder e do botão embed
    const bool hasItems = !bookmarks.isEmpty();
    m_bookmarkList->setVisible(hasItems);
    m_bookmarkEmpty->setVisible(!hasItems);

    if (auto* embedBtn = findChild<QToolButton*>(
            QStringLiteral("bookmarkEmbedBtn")))
        embedBtn->setVisible(hasItems);

    for (const BookmarkEntry& bm : bookmarks)
        addBookmarkItemWidget(bm);
}

// ─────────────────────────────────────────────────────────────────────────────
// addBookmarkItemWidget — adiciona 1 item com widget customizado à QListWidget
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::addBookmarkItemWidget(const BookmarkEntry& bm)
{
    auto* widget = new BookmarkItemWidget(bm.page, bm.label, m_bookmarkList);

    // Navegar
    connect(widget, &BookmarkItemWidget::navigateRequested,
            this, [this](int page) {
        if (m_pdfView) m_pdfView->goToPage(page);
    });

    // Renomear
    connect(widget, &BookmarkItemWidget::renameRequested,
            this, [this](int page, const QString& label) {
        if (m_bookmarkManager) {
            m_bookmarkManager->renameBookmark(page, label);
            saveBookmarksAsync();
        }
    });

    // Remover
    connect(widget, &BookmarkItemWidget::removeRequested,
            this, [this](int page) {
        if (m_bookmarkManager) {
            m_bookmarkManager->removeBookmark(page);
            saveBookmarksAsync();
        }
    });

    auto* item = new QListWidgetItem(m_bookmarkList);
    item->setSizeHint(widget->sizeHint());
    item->setData(Qt::UserRole, bm.page);
    m_bookmarkList->setItemWidget(item, widget);
}

// ─────────────────────────────────────────────────────────────────────────────
// onBookmarkSaveToPdfRequested — embute marcadores no PDF
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onBookmarkSaveToPdfRequested()
{
    if (!m_bookmarkManager) return;
    // Desabilita o botão durante a operação
    if (auto* btn = findChild<QToolButton*>(QStringLiteral("bookmarkEmbedBtn")))
        btn->setEnabled(false);
    statusBar()->showMessage(tr("Embutindo marcadores no PDF…"), 0);
    m_bookmarkManager->saveToPdf();   // sinal pdfSaved() dispara o feedback
}

// ─────────────────────────────────────────────────────────────────────────────
// updateAddBookmarkBtnState — altera aparência do botão ＋ conforme a página
// atual já tem ou não marcador.
//
// • Tem marcador → botão marcado (checked) + texto "Remover marcador"
// • Sem marcador → botão desmarcado + texto "Adicionar marcador"
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateAddBookmarkBtnState(int page)
{
    if (!m_addBookmarkBtn || !m_bookmarkManager) return;

    const bool hasIt = m_bookmarkManager->hasBookmark(page);
    m_addBookmarkBtn->blockSignals(true);
    m_addBookmarkBtn->setChecked(hasIt);
    m_addBookmarkBtn->blockSignals(false);
    m_addBookmarkBtn->setText(hasIt
        ? QString::fromUtf8(UiStr::kBookmarkRemoveBtnText)
        : QString::fromUtf8(UiStr::kBookmarkAddBtnText));
}

// ─────────────────────────────────────────────────────────────────────────────
// saveBookmarksAsync — salva e exibe status bar brevemente.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::saveBookmarksAsync()
{
    if (!m_bookmarkManager || !m_bookmarkManager->isDirty()) return;
    m_bookmarkManager->save();   // emite PdfBookmarkManager::saved()
}

// ─────────────────────────────────────────────────────────────────────────────
// Bookmark slots
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onAddBookmarkClicked()
{
    if (!m_engine || m_engine->type() != DocumentType::PDF) return;
    if (!m_bookmarkManager) return;

    const int page = m_pdfView->currentPage();
    m_bookmarkManager->toggleBookmark(page);  // emite bookmarksChanged
    saveBookmarksAsync();
}

void MainWindow::onBookmarksChanged(const QList<BookmarkEntry>& bm)
{
    populateBookmarkPanel(bm);
    if (m_engine && m_engine->type() == DocumentType::PDF)
        updateAddBookmarkBtnState(m_pdfView->currentPage());
}

void MainWindow::onBookmarkPageStatusChanged(int page)
{
    updateAddBookmarkBtnState(page);
}
