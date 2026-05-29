// main_window_bookmarks.cpp  —  l-reader
//
// Responsabilidades:
//   • Gerenciamento de marcadores (bookmark) via PdfBookmarkManager
//   • Gerenciamento de highlights via HighlightManager
//   • Painel lateral de marcadores e anotações
//   • wireBookmarkSignals() / wireHighlightSignals()

#include "CasualMode/casual_mode_widget.hpp"
#include "bookmark_item_widget.hpp"
#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include "Ui/ui_strings.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QStatusBar>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// wireBookmarkSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireBookmarkSignals() {
  connect(m_bookmarkManager.get(), &PdfBookmarkManager::bookmarksChanged, this,
          [this](const QList<BookmarkEntry> &bm) {
            onBookmarksChanged(bm);
            // Redesenha badges de marcador na galeria
            if (m_galleryList) {
              for (int row = 0; row < m_galleryList->count(); ++row)
                updateGalleryItemBadges(row);
            }
            // Espelha no sidebar do Modo Casual
            if (m_casualWidget)
              m_casualWidget->setBookmarks(bm);
          });

  connect(m_bookmarkManager.get(), &PdfBookmarkManager::saved, this,
          [this](bool ok, const QString &err) {
            if (!ok)
              statusBar()->showMessage(
                  QString::fromUtf8(UiStr::kBookmarkSaveError) +
                      QLatin1Char(' ') + err,
                  5000);
          });

  connect(m_bookmarkManager.get(), &PdfBookmarkManager::pdfSaved, this,
          [this](bool ok, const QString &err) {
            if (ok)
              statusBar()->showMessage(tr("Marcadores embutidos no PDF."),
                                       3000);
            else
              statusBar()->showMessage(tr("Erro ao embutir: ") + err, 6000);
          });

  connect(m_addBookmarkBtn, &QToolButton::clicked, this,
          &MainWindow::onAddBookmarkClicked);

  // ── Sidebar Casual: navegação e remoção a partir dos modelos ─────────
  if (m_casualWidget) {
    // Bookmark: remoção (navegação é tratada em wireDocumentConnections)
    connect(m_casualWidget->bookmarkModel(),
            &CasualBookmarkModel::removeBookmark, this, [this](int page) {
              if (m_bookmarkManager)
                m_bookmarkManager->removeBookmark(page);
            });
    // Anotação: remoção (navegação é tratada em wireDocumentConnections)
    connect(m_casualWidget->annotModel(), &CasualAnnotModel::removeAnnotation,
            this, [this](const QString &id) {
              if (m_highlightManager)
                m_highlightManager->removeHighlight(id);
            });
  }

  // Menu de contexto na lista de marcadores (botão direito)
  m_bookmarkList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_bookmarkList, &QListWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            if (!m_bookmarkList || !m_bookmarkManager)
              return;
            auto *item = m_bookmarkList->itemAt(pos);
            if (!item)
              return;

            auto *w = qobject_cast<BookmarkItemWidget *>(
                m_bookmarkList->itemWidget(item));
            if (!w)
              return;

            QMenu menu(m_bookmarkList);
            auto *actNav =
                menu.addAction(tr("Ir para página %1").arg(w->page() + 1));
            auto *actRename = menu.addAction(tr("Renomear"));
            menu.addSeparator();
            auto *actDel = menu.addAction(tr("Remover marcador"));

            const QAction *chosen =
                menu.exec(m_bookmarkList->viewport()->mapToGlobal(pos));

            if (chosen == actNav)
              if (m_casualPdfView && m_stack->currentIndex() ==
                                         static_cast<int>(ViewIndex::CasualPdf))
                m_casualPdfView->goToSpread(w->page());
              else
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
// wireHighlightSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireHighlightSignals() {
  // Excluir highlight pelo canvas (menu de contexto sobre o highlight)
  connect(m_pdfView, &PdfCanvasView::removeHighlightRequested, this,
          [this](const QString &id) {
            m_highlightManager->removeHighlight(id);
            m_highlightManager->save();
          });

  // Excluir highlight pelo painel lateral (menu de contexto)
  if (m_annotList) {
    m_annotList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_annotList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
              QListWidgetItem *item = m_annotList->itemAt(pos);
              if (!item)
                return;
              const QString id = item->data(Qt::UserRole + 1).toString();
              QMenu menu(this);
              QAction *actDel = menu.addAction(tr("Excluir anotação"));
              connect(actDel, &QAction::triggered, this, [this, id]() {
                m_highlightManager->removeHighlight(id);
                m_highlightManager->save();
              });
              menu.exec(m_annotList->viewport()->mapToGlobal(pos));
            });
  }

  // Canvas solicita criação de highlight
  connect(m_pdfView, &PdfCanvasView::highlightRequested, this,
          &MainWindow::onHighlightRequested);

  // Feedback na status bar
  connect(m_highlightManager.get(), &HighlightManager::saved, this,
          [this](bool ok, const QString &err) {
            if (!ok)
              statusBar()->showMessage(tr("Erro ao salvar highlights: ") + err,
                                       5000);
          });
  connect(m_highlightManager.get(), &HighlightManager::pdfSaved, this,
          [this](bool ok, const QString &err) {
            if (ok)
              statusBar()->showMessage(tr("Highlights embutidos no PDF."),
                                       3000);
            else
              statusBar()->showMessage(tr("Erro ao embutir highlights: ") + err,
                                       6000);
          });

  // highlightsChanged → reconstrói canvas + lista na sidebar + badges na
  // galeria
  connect(m_highlightManager.get(), &HighlightManager::highlightsChanged, this,
          [this](const QVector<HighlightEntry> &highlights) {
            if (!m_pdfView)
              return;

            m_pdfView->clearHighlights();
            for (const auto &h : highlights)
              m_pdfView->addHighlight(h);

            if (m_annotList && m_annotEmpty) {
              m_annotList->clear();
              const bool hasItems = !highlights.isEmpty();
              m_annotList->setVisible(hasItems);
              m_annotEmpty->setVisible(!hasItems);
              for (const auto &h : highlights) {
                const int page = h.firstPage();
                const QString truncated =
                    h.text.length() > 60 ? h.text.left(57) + QStringLiteral("…")
                                         : h.text;
                auto *item = new QListWidgetItem(
                    QStringLiteral("p.%1  %2").arg(page + 1).arg(truncated),
                    m_annotList);
                item->setData(Qt::UserRole, page);
                item->setData(Qt::UserRole + 1, h.id);
                item->setToolTip(h.text);
              }
            }

            // Redesenha badges na galeria
            QSet<int> affectedPages;
            for (const auto &h : highlights)
              affectedPages.insert(h.firstPage());
            for (int p : std::as_const(affectedPages))
              updateGalleryItemBadges(p);

            // Espelha anotações no sidebar do Modo Casual
            if (m_casualWidget)
              m_casualWidget->setHighlights(highlights);
          });

  // Clique em item de anotação → navega até a página
  connect(m_annotList, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            const int page = item->data(Qt::UserRole).toInt();
            // Em Modo Casual PDF, navega no spread; caso contrário na view
            // normal
            if (m_casualPdfView && m_stack->currentIndex() ==
                                       static_cast<int>(ViewIndex::CasualPdf))
              m_casualPdfView->goToSpread(page);
            else if (m_pdfView)
              m_pdfView->goToPage(page);
          });
}

// ─────────────────────────────────────────────────────────────────────────────
// populateBookmarkPanel — reconstrói a lista com BookmarkItemWidget
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::populateBookmarkPanel(const QList<BookmarkEntry> &bookmarks) {
  if (!m_bookmarkList || !m_bookmarkEmpty)
    return;

  m_bookmarkList->clear();

  const bool hasItems = !bookmarks.isEmpty();
  m_bookmarkList->setVisible(hasItems);
  m_bookmarkEmpty->setVisible(!hasItems);

  for (const BookmarkEntry &bm : bookmarks)
    addBookmarkItemWidget(bm);
}

// ─────────────────────────────────────────────────────────────────────────────
// addBookmarkItemWidget — adiciona 1 item com widget customizado à QListWidget
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::addBookmarkItemWidget(const BookmarkEntry &bm) {
  auto *widget = new BookmarkItemWidget(bm.page, bm.label, m_bookmarkList);

  connect(widget, &BookmarkItemWidget::navigateRequested, this,
          [this](int page) {
            // Em Modo Casual PDF, navega no spread; caso contrário na view
            // normal
            if (m_casualPdfView && m_stack->currentIndex() ==
                                       static_cast<int>(ViewIndex::CasualPdf))
              m_casualPdfView->goToSpread(page);
            else if (m_pdfView)
              m_pdfView->goToPage(page);
          });

  connect(widget, &BookmarkItemWidget::renameRequested, this,
          [this](int page, const QString &label) {
            if (m_bookmarkManager) {
              m_bookmarkManager->renameBookmark(page, label);
              saveBookmarksAsync();
            }
          });

  connect(widget, &BookmarkItemWidget::removeRequested, this, [this](int page) {
    if (m_bookmarkManager) {
      m_bookmarkManager->removeBookmark(page);
      saveBookmarksAsync();
    }
  });

  auto *item = new QListWidgetItem(m_bookmarkList);
  item->setSizeHint(widget->sizeHint());
  item->setData(Qt::UserRole, bm.page);
  m_bookmarkList->setItemWidget(item, widget);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateAddBookmarkBtnState — altera aparência do botão + conforme a página
// atual já tem ou não marcador.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateAddBookmarkBtnState(int page) {
  if (!m_addBookmarkBtn || !m_bookmarkManager)
    return;

  const bool hasIt = m_bookmarkManager->hasBookmark(page);
  m_addBookmarkBtn->blockSignals(true);
  m_addBookmarkBtn->setChecked(hasIt);
  m_addBookmarkBtn->blockSignals(false);
  m_addBookmarkBtn->setText(
      hasIt ? QString::fromUtf8(UiStr::kBookmarkRemoveBtnText)
            : QString::fromUtf8(UiStr::kBookmarkAddBtnText));
}

// ─────────────────────────────────────────────────────────────────────────────
// saveBookmarksAsync — persiste e exibe feedback na status bar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::saveBookmarksAsync() {
  if (!m_bookmarkManager || !m_bookmarkManager->isDirty())
    return;
  m_bookmarkManager->save();
}

// ─────────────────────────────────────────────────────────────────────────────
// Bookmark slots
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onAddBookmarkClicked() {
  if (!m_engine || m_engine->type() != DocumentType::PDF)
    return;
  if (!m_bookmarkManager)
    return;

  const int page = m_pdfView->currentPage();
  m_bookmarkManager->toggleBookmark(page);
  saveBookmarksAsync();
}

void MainWindow::onBookmarksChanged(const QList<BookmarkEntry> &bm) {
  populateBookmarkPanel(bm);
  if (m_engine && m_engine->type() == DocumentType::PDF)
    updateAddBookmarkBtnState(m_pdfView->currentPage());
}

void MainWindow::onBookmarkPageStatusChanged(int page) {
  updateAddBookmarkBtnState(page);
}

// ─────────────────────────────────────────────────────────────────────────────
// onHighlightRequested — delega ao HighlightManager e abre painel de anotações
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onHighlightRequested(const HighlightEntry &h) {
  if (!m_highlightManager)
    return;

  m_highlightManager->addHighlight(h);
  m_highlightManager->save();

  // forceOpen=true: garante que o sidebar abre, nunca fecha por toggle
  showSidePanel(SidePanel::Edit, /*forceOpen=*/true);
}
