// main_window_document.cpp  —  l-reader
//
// Responsabilidades:
//   • openDocument() — carregamento e inicialização de PDF / EPUB / MD
//   • switchView()   — alterna o widget central
//   • onModeChanged() — reage à mudança de modo de leitura
//   • setSidebarCasualMode() — adapta o sidebar ao modo Casual
//   • wireModeSignals() / wireDocumentSignals()

#include "DocumentEngine/ebook_engine.hpp"
#include "DocumentEngine/pdf_engine.hpp"
#include "bookmark_item_widget.hpp"
#include "casual_pdf_view.hpp"
#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include "Ui/ui_strings.hpp"

#include <QButtonGroup>
#include <QDebug>
#include <QFileInfo>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStatusBar>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// openDocument
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::openDocument(const QString &path) {
  // Estilo Okular: ao abrir novo documento, salva o atual no PDF.
  autoSaveToPdf();

  m_engine = DocumentEngine::create(path, this);

  if (!m_engine) {
    setWindowTitle(QStringLiteral("l-reader — formato não suportado"));
    m_sbFile->setText(
        tr("Formato não suportado: %1").arg(QFileInfo(path).fileName()));
    return;
  }

  connect(
      m_engine.get(), &DocumentEngine::documentLoaded, this,
      [this](const QString &p, int pages) {
        const QString fileName = QFileInfo(p).fileName();
        setWindowTitle(QStringLiteral("l-reader — %1").arg(fileName));

        if (m_engine->type() == DocumentType::PDF) {
          auto *pdf = qobject_cast<PDFEngine *>(m_engine.get());
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

          if (m_bookTitleLabel) {
            // Exibe título legível: remove extensão e substitui _ / - por
            // espaços
            QString cleanTitle = QFileInfo(p).completeBaseName();
            cleanTitle.replace(QLatin1Char('_'), QLatin1Char(' '));
            cleanTitle.replace(QLatin1Char('-'), QLatin1Char(' '));
            // Remove prefixos típicos de filenames de livros: "[Autor]",
            // "(2023)" etc.
            cleanTitle.remove(
                QRegularExpression(QStringLiteral("^\\[.*?\\]\\s*")));
            cleanTitle = cleanTitle.trimmed();
            m_bookTitleLabel->setText(cleanTitle);
          }
          if (m_bookAuthorLabel)
            m_bookAuthorLabel->setText(QString());

          populateGallery(pages);

          const int savedPage = m_sidecar->loadPagePosition();
          if (savedPage > 0 && savedPage < pages)
            m_pdfView->goToPage(savedPage);

        } else {
          auto *ebook = qobject_cast<EBookEngine *>(m_engine.get());
          if (!ebook || ebook->spineUrls().isEmpty()) {
            m_sbFile->setText(tr("Erro: EPUB sem capítulos"));
            return;
          }
          m_spineUrls = ebook->spineUrls();
          m_currentChapter = 0;
          m_webView->setZoomFactor(1.0);
          switchView(ViewIndex::Web);
          const QString title =
              ebook->title().isEmpty() ? fileName : ebook->title();
          m_sbFile->setText(title);
          updatePageIndicator(0, m_spineUrls.size());
          m_actPrevPage->setEnabled(false);
          m_actNextPage->setEnabled(m_spineUrls.size() > 1);
          m_actZoomIn->setEnabled(true);
          m_actZoomOut->setEnabled(true);
          m_actZoomReset->setEnabled(true);
          m_zoomLabel->setText(QString::fromLatin1(UiStr::kZoomDefault));
          loadEpubChapter(0);

          if (m_bookTitleLabel)
            m_bookTitleLabel->setText(title);
          if (m_bookAuthorLabel)
            m_bookAuthorLabel->setText(ebook->title());
        }

        // ── TOC assíncrona ────────────────────────────────────────────────
        if (m_sidebarTocLbl)
          m_sidebarTocLbl->setText(tr("Carregando sumário…"));
        if (m_tocTree)
          m_tocTree->clear();
        m_tocWorker->extract(m_engine.get());

        // ── Marcadores (somente PDF) ───────────────────────────────────────
        if (m_engine->type() == DocumentType::PDF) {
          auto *pdf = qobject_cast<PDFEngine *>(m_engine.get());
          m_bookmarkManager->setDocument(pdf->rawDocument(), p, notesDir());
          m_highlightManager->setDocument(pdf->rawDocument(), p, notesDir());
          if (m_addBookmarkBtn)
            m_addBookmarkBtn->setEnabled(true);
          updateAddBookmarkBtnState(0);
        } else {
          m_bookmarkManager->clear();
          m_highlightManager->setDocument(nullptr, p, notesDir());
          if (m_addBookmarkBtn)
            m_addBookmarkBtn->setEnabled(false);
          populateBookmarkPanel({});
        }

        m_sidecar->trackDocument(p);
        m_modeManager->transitionTo(ModeType::Standard);

        // ── Modo Casual — alimenta o controller ───────────────────────────
        const int startPage =
            m_sidecar ? std::max(0, m_sidecar->loadPagePosition()) : 0;
        m_casualWidget->controller()->setDocument(m_engine.get(), startPage);
      });

  connect(m_engine.get(), &DocumentEngine::loadFailed, this,
          [this](const QString &error) {
            setWindowTitle(QStringLiteral("l-reader — erro ao abrir"));
            m_sbFile->setText(tr("Erro ao abrir documento"));
            qWarning() << "[l-reader] loadFailed:" << error;
          });

  m_engine->load(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// switchView — alterna o widget central (PDF / Web / Casual)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::switchView(ViewIndex idx) {
  m_stack->setCurrentIndex(static_cast<int>(idx));
}

// ─────────────────────────────────────────────────────────────────────────────
// onModeChanged — reage à transição de modo de leitura
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onModeChanged(ModeType newMode, ModeType /*previous*/) {
  m_actModeStd->setChecked(newMode == ModeType::Standard);
  m_actModeStudy->setChecked(newMode == ModeType::Study);
  m_actModeCasual->setChecked(newMode == ModeType::Casual);

  if (m_modeDropdown && m_modeManager)
    m_modeDropdown->setText(m_modeManager->currentName());

  if (m_sbMode && m_modeManager)
    m_sbMode->setText(m_modeManager->currentName());

  if (newMode == ModeType::Casual && m_engine) {
    const DocumentType dtype = m_engine->type();

    if (dtype == DocumentType::PDF) {
      // ── Modo Casual + PDF → spread de 2 páginas ───────────────────
      auto *pdf = qobject_cast<PDFEngine *>(m_engine.get());

      const int currentPage = m_pdfView->currentPage();
      const int spreadStart = (currentPage / 2) * 2;

      // Passa rawDocument() para habilitar seleção de texto e highlights
      // no spread view. A assinatura mudou de (cache, pageCount) para
      // (cache, doc, pageCount) após a extração de PdfTextLayer.
      m_casualPdfView->setPageCache(m_pageCache.get(),
                                    pdf ? pdf->rawDocument() : nullptr,
                                    m_pdfView->pageCount());

      // Ajusta DPI para spread ANTES de goToSpread —
      // kCasualDpi (110) gera renders 2-3× mais rápidos que 150 DPI.
      m_casualPdfView->activateDpi();
      m_casualPdfView->goToSpread(spreadStart);

      // Sincroniza indicadores da toolbar ao virar página.
      // disconnect primeiro para evitar duplicação se entrar no modo
      // Casual mais de uma vez sem fechar o documento.
      disconnect(m_casualPdfView, &CasualPdfView::spreadChanged, this, nullptr);
      connect(m_casualPdfView, &CasualPdfView::spreadChanged, this,
              [this](int left, int /*right*/) {
                updatePageIndicator(left, m_pdfView->pageCount());
                m_actPrevPage->setEnabled(left > 0);
                m_actNextPage->setEnabled(left + 2 < m_pdfView->pageCount());
                if (m_sidecar)
                  m_sidecar->savePagePosition(left);
                setSidebarCasualMode(true);
              });

      // ── Wiring de highlights para o spread (espelha PdfCanvasView) ─
      // Desconecta primeiro para evitar conexões duplicadas em re-entrada.
      disconnect(m_casualPdfView, &CasualPdfView::highlightRequested, this,
                 nullptr);
      disconnect(m_casualPdfView, &CasualPdfView::removeHighlightRequested,
                 this, nullptr);
      connect(m_casualPdfView, &CasualPdfView::highlightRequested,
              m_highlightManager.get(), &HighlightManager::addHighlight);
      connect(m_casualPdfView, &CasualPdfView::removeHighlightRequested,
              m_highlightManager.get(), &HighlightManager::removeHighlight);

      // Sincroniza highlights existentes no spread recém-aberto
      m_casualPdfView->clearHighlights();
      for (const auto &h : m_highlightManager->highlights())
        m_casualPdfView->addHighlight(h);

      // Quando HighlightManager persiste uma mudança (ex: highlight criado
      // no modo Standard), atualiza o spread sem precisar sair e entrar.
      disconnect(m_highlightManager.get(), &HighlightManager::highlightsChanged,
                 m_casualPdfView, nullptr);
      connect(m_highlightManager.get(), &HighlightManager::highlightsChanged,
              m_casualPdfView, [this](const QVector<HighlightEntry> &hls) {
                m_casualPdfView->clearHighlights();
                for (const auto &h : hls)
                  m_casualPdfView->addHighlight(h);
              });

      switchView(ViewIndex::CasualPdf);
      m_casualPdfView->setFocus();

    } else if (dtype == DocumentType::EPUB || dtype == DocumentType::MOBI) {
      // ── Modo Casual + EPUB → CasualModeWidget (QML) ──────────────
      const int currentPos = m_currentChapter;
      m_casualWidget->controller()->setDocument(m_engine.get(), currentPos);
      switchView(ViewIndex::Casual);
    }
  }

  // Ao sair do Modo Casual:
  // PDF → volta para ViewIndex::Pdf
  if (newMode != ModeType::Casual && m_engine) {
    if (m_engine->type() == DocumentType::PDF &&
        m_stack->currentIndex() == static_cast<int>(ViewIndex::CasualPdf)) {
      const int page = m_casualPdfView->currentLeftPage();
      m_casualPdfView->deactivateDpi(); // restaura DPI do modo normal
      switchView(ViewIndex::Pdf);
      m_pdfView->goToPage(page);
    }
    // EPUB → volta para ViewIndex::Web
    if ((m_engine->type() == DocumentType::EPUB ||
         m_engine->type() == DocumentType::MOBI) &&
        m_stack->currentIndex() == static_cast<int>(ViewIndex::Casual)) {
      switchView(ViewIndex::Web);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// setSidebarCasualMode — alterna conteúdo do sidebar entre modos
//
// Modo Casual:
//   visível  → bookInfoWidget (capa + título + progresso)  +  TOC (m_sideStack)
//   oculto   → sideIconBar, sidebarTocLbl, addBookmarkBtn
//
// Outros modos:
//   visível  → sideIconBar + m_sideStack (painel ativo)  +  addBookmarkBtn
//   oculto   → bookInfoWidget
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setSidebarCasualMode(bool casual) {
  // ── Visibilidade dos elementos ─────────────────────────────────────────
  if (m_bookInfoWidget)
    m_bookInfoWidget->setVisible(casual);
  if (m_sideIconBar)
    m_sideIconBar->setVisible(!casual);
  if (m_sidebarTocLbl)
    m_sidebarTocLbl->setVisible(!casual);
  if (m_addBookmarkBtn)
    m_addBookmarkBtn->setVisible(!casual);
  if (m_casualTabBar)
    m_casualTabBar->setVisible(casual);

  // Separadores relacionados ao header e tab bar casual
  auto *headerSep = m_bookInfoWidget
                        ? m_bookInfoWidget->parentWidget()->findChild<QFrame *>(
                              QStringLiteral("casualHeaderSep"))
                        : nullptr;
  if (headerSep)
    headerSep->setVisible(casual);

  auto *tabSep = m_casualTabBar
                     ? m_casualTabBar->parentWidget()->findChild<QFrame *>(
                           QStringLiteral("casualTabSep"))
                     : nullptr;
  if (tabSep)
    tabSep->setVisible(casual);

  // ── m_sideStack: sempre visível; em casual mostra o TOC por padrão
  if (m_sideStack) {
    m_sideStack->setVisible(true);
    if (casual) {
      m_sideStack->setCurrentIndex(static_cast<int>(SidePanel::Toc));
      // Reseta seleção da tab bar para "Sumário"
      if (m_casualTabBar) {
        const auto btns = m_casualTabBar->findChildren<QToolButton *>();
        for (auto *b : btns)
          b->setChecked(b->objectName() == QLatin1String("casualBtnToc"));
      }
    }
  }

  // ── TOC: sem decorações de árvore no modo Casual (lista flat)
  if (m_tocTree) {
    m_tocTree->setRootIsDecorated(!casual);
    m_tocTree->setProperty("casualMode", casual);
    m_tocTree->style()->unpolish(m_tocTree);
    m_tocTree->style()->polish(m_tocTree);
  }

  // ── Sincroniza a barra de progresso lateral com o estado atual
  if (casual && m_engine) {
    const int total = (m_engine->type() == DocumentType::PDF)
                          ? m_pdfView->pageCount()
                          : m_spineUrls.size();
    const int current = (m_engine->type() == DocumentType::PDF)
                            ? m_pdfView->currentPage()
                            : m_currentChapter;

    const int pct =
        (total > 1) ? static_cast<int>(
                          (static_cast<double>(current) / (total - 1)) * 100.0)
                    : 0;

    if (auto *bar = m_bookInfoWidget->findChild<QProgressBar *>(
            QStringLiteral("casualSideProgressBar")))
      bar->setValue(pct);

    if (auto *lbl = m_bookInfoWidget->findChild<QLabel *>(
            QStringLiteral("casualSideProgressPct")))
      lbl->setText(QStringLiteral("%1%").arg(pct));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// wireModeSignals — sinais do ModeManager e dos botões de modo
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireModeSignals() {
  connect(m_modeManager.get(), &ModeManager::modeChanged, this,
          &MainWindow::onModeChanged);

  connect(m_actModeStd, &QAction::triggered, this,
          [this] { m_modeManager->transitionTo(ModeType::Standard); });
  connect(m_actModeStudy, &QAction::triggered, this,
          [this] { m_modeManager->transitionTo(ModeType::Study); });
  connect(m_actModeCasual, &QAction::triggered, this,
          [this] { m_modeManager->transitionTo(ModeType::Casual); });
}

// ─────────────────────────────────────────────────────────────────────────────
// wireDocumentSignals — ações de abrir documento e navegação global
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireDocumentSignals() {
  connect(m_actOpen, &QAction::triggered, this, &MainWindow::openFileDialog);

  // Sidebar toggle
  if (auto *btn = findChild<QToolButton *>(QStringLiteral("sidebarToggleBtn")))
    connect(btn, &QToolButton::clicked, this,
            [this] { m_sidebar->setVisible(!m_sidebar->isVisible()); });

  // Navegação prev/next (PDF e EPUB)
  connect(m_actPrevPage, &QAction::triggered, this, [this] {
    const int idx = m_stack->currentIndex();
    if (idx == static_cast<int>(ViewIndex::Web))
      loadEpubChapter(m_currentChapter - 1);
    else if (idx == static_cast<int>(ViewIndex::CasualPdf))
      m_casualPdfView->prevSpread();
    else
      m_pdfView->goToPage(m_pdfView->currentPage() - 1);
  });
  connect(m_actNextPage, &QAction::triggered, this, [this] {
    const int idx = m_stack->currentIndex();
    if (idx == static_cast<int>(ViewIndex::Web))
      loadEpubChapter(m_currentChapter + 1);
    else if (idx == static_cast<int>(ViewIndex::CasualPdf))
      m_casualPdfView->nextSpread();
    else
      m_pdfView->goToPage(m_pdfView->currentPage() + 1);
  });

  // Casual bottom-nav espelha prev/next
  connect(m_actCasualPrev, &QAction::triggered, this,
          [this] { m_actPrevPage->trigger(); });
  connect(m_actCasualNext, &QAction::triggered, this,
          [this] { m_actNextPage->trigger(); });

  // Slider de progresso
  connect(m_progressSlider, &QSlider::valueChanged, this, [this](int val) {
    if (!m_progressLabel)
      return;
    m_progressLabel->setText(QStringLiteral("%1%").arg(val));
  });

  // Zoom
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

  // Ferramentas de anotação (Pan / Select / Annotate)
  {
    auto *toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);

    auto wireToolBtn = [&](const char *name, PdfCanvasView::ToolMode mode,
                           std::function<void(bool)> extraOnChecked = {}) {
      auto *btn = findChild<QToolButton *>(QString::fromLatin1(name));
      if (!btn)
        return;
      btn->setCheckable(true);
      toolGroup->addButton(btn);

      connect(btn, &QToolButton::toggled, this,
              [this, mode, extraOnChecked](bool checked) {
                if (!m_pdfView)
                  return;
                if (checked)
                  m_pdfView->setToolMode(mode);
                if (extraOnChecked)
                  extraOnChecked(checked);
              });

      connect(m_pdfView, &PdfCanvasView::toolModeChanged, btn,
              [btn, mode](PdfCanvasView::ToolMode current) {
                QSignalBlocker blocker(btn);
                btn->setChecked(current == mode);
              });
    };

    wireToolBtn("toolMoveBtn", PdfCanvasView::ToolMode::Pan);
    wireToolBtn("toolSelectBtn", PdfCanvasView::ToolMode::Select);
    wireToolBtn("toolAnnotateBtn", PdfCanvasView::ToolMode::Annotate,
                [this](bool checked) {
                  if (checked)
                    showSidePanel(SidePanel::Edit, /*forceOpen=*/true);
                });

    if (auto *btn = findChild<QToolButton *>(QStringLiteral("toolSelectBtn"))) {
      QSignalBlocker blocker(btn);
      btn->setChecked(true);
    }
  }

  // Modo Casual — navegação de capítulo pedida pelo QML
  connect(m_casualWidget->controller(),
          &CasualModeController::chapterNavigationRequested, this,
          [this](int chapterIndex) { loadEpubChapter(chapterIndex); });
}
