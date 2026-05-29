// main_window_page.cpp  —  l-reader
//
// Responsabilidades:
//   • Slots de mudança de página (onPageChanged) e zoom (onZoomChanged/Settled)
//   • Atualização de indicadores visuais (pageLabel, progressLabel)
//   • wirePageZoomSignals()

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include "Ui/ui_strings.hpp"

#include <QProgressBar>
#include <QScrollBar>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// wirePageZoomSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wirePageZoomSignals() {
  connect(m_pdfView, &PdfCanvasView::currentPageChanged, this,
          &MainWindow::onPageChanged);
  connect(m_pdfView, &PdfCanvasView::zoomChanged, this,
          &MainWindow::onZoomChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// onPageChanged — sincroniza indicadores e persiste posição de leitura
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onPageChanged(int page) {
  const int total = m_pdfView->pageCount();
  updatePageIndicator(page, total);
  m_actPrevPage->setEnabled(page > 0);
  m_actNextPage->setEnabled(page < total - 1);
  highlightTocEntry(page);
  onBookmarkPageStatusChanged(page);

  if (m_sidecar)
    m_sidecar->savePagePosition(page);

  if (m_casualWidget) {
    m_casualWidget->controller()->setCurrentPage(page);
    m_casualWidget->setCurrentPage(
        page); // actualiza highlight de TOC no sidebar
  }

  // ── Atualiza a barra de progresso lateral do Modo Casual ─────────────
  if (m_bookInfoWidget && m_bookInfoWidget->isVisible() && total > 1) {
    const int pct =
        static_cast<int>((static_cast<double>(page) / (total - 1)) * 100.0);
    if (auto *bar = m_bookInfoWidget->findChild<QProgressBar *>(
            QStringLiteral("casualSideProgressBar")))
      bar->setValue(pct);
    if (auto *lbl = m_bookInfoWidget->findChild<QLabel *>(
            QStringLiteral("casualSideProgressPct")))
      lbl->setText(QStringLiteral("%1%").arg(pct));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// onZoomChanged — atualiza label imediatamente; adia atualização do cache
// via debounce (evita re-renders para níveis intermediários da animação).
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onZoomChanged(qreal zoom) {
  m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
  m_pendingZoom = zoom;
  if (m_zoomDebounceTimer)
    m_zoomDebounceTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// onZoomSettled — disparado após kZoomDebounceMs de silêncio.
// Só aqui atualiza o DPI do cache e solicita re-render com resolução correta.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onZoomSettled() {
  if (m_pageCache && m_engine && m_engine->type() == DocumentType::PDF) {
    constexpr qreal BASE_DPI = 150.0;
    m_pageCache->setDpi(BASE_DPI * m_pendingZoom);
    if (m_pdfView)
      m_pageCache->onCurrentPageChanged(m_pdfView->currentPage());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// updatePageIndicator
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updatePageIndicator(int page, int total) {
  if (total > 0) {
    m_pageLabel->setText(QStringLiteral("%1 / %2").arg(page + 1).arg(total));
    updateProgressLabel(page, total);
  } else {
    m_pageLabel->setText(QString::fromLatin1(UiStr::kPageDefault));
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateProgressLabel — atualiza slider e label de porcentagem
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateProgressLabel(int page, int total) {
  if (!m_progressSlider || total <= 0)
    return;
  const int pct =
      static_cast<int>((static_cast<double>(page) / (total - 1)) * 100.0);
  m_progressSlider->setValue(pct);
  if (m_progressLabel)
    m_progressLabel->setText(QStringLiteral("%1%").arg(pct));
}
