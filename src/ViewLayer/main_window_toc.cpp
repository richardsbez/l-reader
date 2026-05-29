// main_window_toc.cpp  —  l-reader
//
// Responsabilidades:
//   • Construção e atualização da árvore de sumário (populateTocTree)
//   • Sincronização de seleção ao navegar (highlightTocEntry)
//   • Controle dos painéis laterais (showSidePanel, onSidePanelRequested)
//   • Slots do TocWorker (onTocReady, onTocEmpty)
//   • wireTocSignals()

#include "CasualMode/casual_mode_widget.hpp"
#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <algorithm>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// wireTocSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireTocSignals() {
  connect(m_tocWorker.get(), &TocWorker::tocReady, this,
          &MainWindow::onTocReady);
  connect(m_tocWorker.get(), &TocWorker::tocEmpty, this,
          &MainWindow::onTocEmpty);

  connect(m_tocTree, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem *item, int) {
            if (!item || !m_engine)
              return;

            if (m_engine->type() == DocumentType::PDF) {
              const int page = item->data(0, Qt::UserRole).toInt();
              if (page >= 0) {
                // Em Modo Casual PDF, navega no spread (duas páginas);
                // nos outros modos, usa a view normal de página única.
                if (m_casualPdfView &&
                    m_stack->currentIndex() ==
                        static_cast<int>(ViewIndex::CasualPdf))
                  m_casualPdfView->goToSpread(page);
                else
                  m_pdfView->goToPage(page);
              }
              return;
            }

            // EPUB: navega para a URL armazenada no item
            const QUrl url = item->data(0, Qt::UserRole).toUrl();
            if (!url.isValid())
              return;

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
}

// ─────────────────────────────────────────────────────────────────────────────
// populateTocTree
//
// Para PDF : entry.page >= 0, entry.url inválida → Qt::UserRole armazena int
// Para EPUB: entry.url válida, entry.page == -1  → Qt::UserRole armazena QUrl
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::populateTocTree(const QList<TocEntry> &entries) {
  if (!m_tocTree)
    return;
  m_tocTree->clear();

  if (entries.isEmpty()) {
    // Fallback EPUB: usa spine diretamente
    for (int i = 0; i < m_spineUrls.size(); ++i) {
      auto *item = new QTreeWidgetItem(m_tocTree);
      item->setText(0, tr("Capítulo %1").arg(i + 1));
      item->setData(0, Qt::UserRole, m_spineUrls[i]);
    }
  } else {
    QVector<QTreeWidgetItem *> stack;
    for (const TocEntry &entry : entries) {
      auto *item = new QTreeWidgetItem();
      item->setText(0,
                    entry.title.isEmpty() ? tr("(sem título)") : entry.title);
      item->setToolTip(0, entry.title);

      if (entry.page >= 0)
        item->setData(0, Qt::UserRole, entry.page);
      else
        item->setData(0, Qt::UserRole, entry.url);

      const int depth = std::max(0, entry.depth);
      while (stack.size() > depth)
        stack.removeLast();

      // Hierarquia visual por profundidade
      {
        QFont f = item->font(0);
        if (depth == 0) {
          f.setWeight(QFont::Medium);
          item->setForeground(0, QColor(0xB3, 0xB3, 0xB3));
        } else if (depth == 1) {
          f.setWeight(QFont::Normal);
          item->setForeground(0, QColor(0x75, 0x75, 0x75));
        } else {
          f.setWeight(QFont::Normal);
          item->setForeground(0, QColor(0x50, 0x50, 0x50));
        }
        item->setFont(0, f);
      }

      if (stack.isEmpty())
        m_tocTree->addTopLevelItem(item);
      else
        stack.last()->addChild(item);

      stack.append(item);
    }
  }

  m_tocTree->collapseAll();
}

// ─────────────────────────────────────────────────────────────────────────────
// highlightTocEntry — seleciona o último item cujo page <= currentPage (PDF).
// Comportamento "rolling" — como leitores de eBook.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::highlightTocEntry(int page) {
  if (!m_tocTree || !m_engine)
    return;
  if (m_engine->type() != DocumentType::PDF)
    return;

  QTreeWidgetItem *best = nullptr;
  int bestPage = -1;

  std::function<void(QTreeWidgetItem *)> dfs = [&](QTreeWidgetItem *it) {
    const int itemPage = it->data(0, Qt::UserRole).toInt();
    if (itemPage >= 0 && itemPage <= page && itemPage > bestPage) {
      best = it;
      bestPage = itemPage;
    }
    for (int c = 0; c < it->childCount(); ++c)
      dfs(it->child(c));
  };
  for (int i = 0; i < m_tocTree->topLevelItemCount(); ++i)
    dfs(m_tocTree->topLevelItem(i));

  if (best) {
    m_tocTree->blockSignals(true);
    m_tocTree->setCurrentItem(best);
    m_tocTree->scrollToItem(best, QAbstractItemView::EnsureVisible);
    m_tocTree->blockSignals(false);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// showSidePanel — abre o sidebar e troca para o painel indicado.
// forceOpen=false: toggle (fecha se já está nesse painel).
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::showSidePanel(SidePanel panel, bool forceOpen) {
  const int idx = static_cast<int>(panel);
  auto *dock = qobject_cast<QDockWidget *>(m_sidebar);
  if (!dock)
    return;

  if (!forceOpen && dock->isVisible() && m_sideStack->currentIndex() == idx) {
    dock->hide();
    return;
  }

  m_sideStack->setCurrentIndex(idx);
  dock->show();
  dock->raise();
}

// ─────────────────────────────────────────────────────────────────────────────
// onSidePanelRequested — disparado pelos botões da sideIconBar
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onSidePanelRequested(const QString &btnObjectName) {
  static const QHash<QString, SidePanel> kMap = {
      {QStringLiteral("sideBtnList"), SidePanel::Toc},
      {QStringLiteral("sideBtnSearch"), SidePanel::Search},
      {QStringLiteral("sideBtnGallery"), SidePanel::Gallery},
      {QStringLiteral("sideBtnEdit"), SidePanel::Edit},
      {QStringLiteral("sideBtnBookmark"), SidePanel::Bookmarks},
  };

  const auto it = kMap.find(btnObjectName);
  if (it == kMap.end())
    return;

  showSidePanel(it.value(), /*forceOpen=*/false);

  // Atualiza estado visual dos botões
  const auto buttons = m_sideIconBar->findChildren<QToolButton *>();
  for (auto *btn : buttons)
    btn->setChecked(btn->objectName() == btnObjectName &&
                    qobject_cast<QDockWidget *>(m_sidebar)->isVisible());
}

// ─────────────────────────────────────────────────────────────────────────────
// onTocReady / onTocEmpty — slots do TocWorker
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onTocReady(QList<TocEntry> entries) {
  if (m_sidebarTocLbl)
    m_sidebarTocLbl->setText(tr("Sumário"));
  populateTocTree(entries);

  // Alimenta também o sidebar do Modo Casual
  if (m_casualWidget)
    m_casualWidget->setTocEntries(entries);
}

void MainWindow::onTocEmpty() {
  if (m_sidebarTocLbl)
    m_sidebarTocLbl->setText(tr("Sumário indisponível"));
  populateTocTree({});

  if (m_casualWidget)
    m_casualWidget->setTocEntries({});
}
