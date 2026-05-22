// main_window_gallery.cpp  —  l-reader
//
// Responsabilidades:
//   • Galeria de miniaturas via PageCache (sem extração Poppler direta)
//   • Carregamento em lotes por idle do event loop (populateGallery)
//   • Atualização incremental de thumbs (onGalleryPageReady)
//   • Renderização de badges (marcador / highlight) sobre miniaturas
//   • wireGallerySignals()

#include "main_window.hpp"
#include "pdf_canvas_view.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QPainter>
#include <QTimer>
#include <memory>

// ── Dimensões das miniaturas ──────────────────────────────────────────────────
static constexpr int kThumbW = 160;
static constexpr int kThumbH = 226;   // ~A4

// ─────────────────────────────────────────────────────────────────────────────
// Helpers estáticos de pixel
// ─────────────────────────────────────────────────────────────────────────────

// Cria um QPixmap placeholder cinza com número de página sobreposto
static QPixmap makePlaceholderThumb(int page)
{
    QPixmap px(kThumbW, kThumbH);
    px.fill(QColor(40, 40, 40));
    QPainter p(&px);
    p.setPen(QColor(90, 90, 90));
    p.setFont(QFont(QStringLiteral("sans-serif"), 11));
    p.drawText(px.rect(), Qt::AlignCenter, QString::number(page + 1));
    return px;
}

// Escala um pixmap pleno do cache para tamanho de miniatura
static QPixmap scaleToThumb(const QPixmap& full)
{
    return full.scaled(kThumbW, kThumbH,
                       Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);
}

// ─────────────────────────────────────────────────────────────────────────────
// paintBadges — desenha círculos de badge (marcador=azul, highlight=amarelo)
// ─────────────────────────────────────────────────────────────────────────────
QPixmap MainWindow::paintBadges(const QPixmap& thumb, int page) const
{
    const bool hasBm = m_bookmarkManager && m_bookmarkManager->hasBookmark(page);
    const bool hasHl = m_highlightManager && [&]{
        for (const auto& h : m_highlightManager->highlights())
            if (h.firstPage() == page) return true;
        return false;
    }();

    if (!hasBm && !hasHl) return thumb;

    QPixmap result = thumb;
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);

    int x = kThumbW - 10;
    const int y = 8;
    const int r = 6;

    if (hasBm) {
        p.setBrush(QColor(70, 130, 220));
        p.setPen(Qt::NoPen);
        p.drawEllipse(x - r, y - r, r * 2, r * 2);
        x -= (r * 2 + 4);
    }
    if (hasHl) {
        p.setBrush(QColor(255, 210, 50));
        p.setPen(Qt::NoPen);
        p.drawEllipse(x - r, y - r, r * 2, r * 2);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// wireGallerySignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireGallerySignals()
{
    connect(m_pageCache.get(), &PageCache::pageReady,
            m_pdfView, &PdfCanvasView::requestRepaintPage);

    connect(m_pageCache.get(), &PageCache::pageReady,
            this, &MainWindow::onGalleryPageReady);

    // Sincroniza seleção da galeria com a página atual do canvas
    connect(m_pdfView, &PdfCanvasView::currentPageChanged,
            this, [this](int page) {
        if (m_galleryList && m_galleryList->count() > page) {
            m_galleryList->setCurrentRow(page);
            scheduleVisibleGalleryThumbs();
        }
    });

    // Clicar numa miniatura navega para a página
    connect(m_galleryList, &QListWidget::itemActivated,
            this, [this](QListWidgetItem* item) {
        if (item && m_pdfView)
            m_pdfView->goToPage(item->data(Qt::UserRole).toInt());
    });

    // Scroll da galeria agenda renders para itens que entraram na tela
    connect(m_galleryList->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::scheduleVisibleGalleryThumbs);
}

// ─────────────────────────────────────────────────────────────────────────────
// populateGallery — cria N itens com placeholder em lotes via idle do event
// loop, evitando travamento da UI em PDFs com centenas de páginas.
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::populateGallery(int pageCount)
{
    if (!m_galleryList || !m_galleryEmpty) return;

    m_galleryList->clear();

    if (pageCount <= 0) {
        m_galleryList->hide();
        m_galleryEmpty->show();
        return;
    }

    m_galleryList->show();
    m_galleryEmpty->hide();
    m_galleryList->setUniformItemSizes(true);

    constexpr int kBatchSize = 50;

    auto nextPage = std::make_shared<int>(0);

    // shared_ptr<std::function> permite auto-referência segura do lambda.
    // (padrão evita std::bad_function_call ao chamar recursivamente)
    auto addBatch = std::make_shared<std::function<void()>>();
    *addBatch = [this, pageCount, nextPage, addBatch]() mutable {
        const int start = *nextPage;
        const int end   = std::min(start + kBatchSize, pageCount);

        for (int i = start; i < end; ++i) {
            auto* item = new QListWidgetItem(m_galleryList);
            item->setData(Qt::UserRole, i);
            item->setSizeHint(QSize(kThumbW + 8, kThumbH + 28));
            item->setIcon(QIcon(makePlaceholderThumb(i)));
            item->setText(tr("Pág. %1").arg(i + 1));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        }

        *nextPage = end;

        if (end < pageCount) {
            QTimer::singleShot(0, this, [this, pageCount, nextPage,
                                         addBatch]() mutable { (*addBatch)(); });
        } else {
            scheduleVisibleGalleryThumbs();
        }
    };

    QTimer::singleShot(0, this, [addBatch]() mutable { (*addBatch)(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// onGalleryPageReady — PageCache entregou uma página; atualiza o item
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onGalleryPageReady(int page)
{
    if (!m_galleryList || page < 0 || page >= m_galleryList->count()) return;

    auto opt = m_pageCache->get(page);
    if (!opt) return;

    QPixmap thumb = paintBadges(scaleToThumb(*opt), page);
    m_galleryList->item(page)->setIcon(QIcon(thumb));
}

// ─────────────────────────────────────────────────────────────────────────────
// scheduleVisibleGalleryThumbs — agenda renders para itens visíveis na lista
// (pool de galeria tem prioridade baixa para não competir com o render principal)
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::scheduleVisibleGalleryThumbs()
{
    if (!m_galleryList || !m_pageCache) return;

    const QRect viewport = m_galleryList->viewport()->rect();
    for (int row = 0; row < m_galleryList->count(); ++row) {
        const QRect r = m_galleryList->visualItemRect(m_galleryList->item(row));
        if (viewport.intersects(r))
            m_pageCache->requestGalleryRender(row);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateGalleryItemBadges — redesenha os badges de um item já renderizado
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::updateGalleryItemBadges(int page)
{
    if (!m_galleryList || page < 0 || page >= m_galleryList->count()) return;

    auto opt = m_pageCache->get(page);
    QPixmap thumb = opt ? scaleToThumb(*opt) : makePlaceholderThumb(page);
    m_galleryList->item(page)->setIcon(QIcon(paintBadges(thumb, page)));
}
