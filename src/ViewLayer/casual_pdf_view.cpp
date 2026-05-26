// casual_pdf_view.cpp  —  l-reader

#include "casual_pdf_view.hpp"
#include "RenderSubsystem/page_cache.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
CasualPdfView::CasualPdfView(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Fade-in ao virar página (0 → 1 em 200ms)
    m_fadeAnim = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnim->setDuration(200);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutQuad);

    // Spinner: gira 30° a cada 40ms → 75 rpm, bem visível sem pesar
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(40);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this] {
        m_spinnerAngle = (m_spinnerAngle + 30) % 360;
        // Repinta só se ainda há páginas sem carregar
        if (!m_leftPx || (!m_rightPx && m_leftPage + 1 < m_pageCount))
            update();
        else
            m_spinnerTimer->stop();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// activateDpi / deactivateDpi
//
// Chamados por MainWindow ao entrar/sair do modo Casual.
// Garante que o cache usa DPI de spread (~110) durante a leitura casual,
// sem interferir com o DPI do PdfCanvasView nos outros modos.
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::activateDpi()
{
    if (!m_cache) return;
    if (m_activeDpi == kCasualDpi) return;
    m_activeDpi = kCasualDpi;
    m_cache->setDpi(kCasualDpi);
    // Invalida cache local — os pixmaps antigos são do DPI anterior
    m_leftPx.reset();
    m_rightPx.reset();
    requestCurrentPages();
}

void CasualPdfView::deactivateDpi()
{
    // DPI do PdfCanvasView será restaurado por MainWindow::onZoomSettled()
    m_activeDpi = 0.0;
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::setPageCache(PageCache* cache, int pageCount)
{
    if (m_cache)
        disconnect(m_cache, nullptr, this, nullptr);

    m_cache     = cache;
    m_pageCount = pageCount;
    m_leftPage  = 0;
    m_leftPx.reset();
    m_rightPx.reset();

    if (m_cache) {
        connect(m_cache, &PageCache::pageReady,
                this,    &CasualPdfView::onPageReady);
    }
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::setBackgroundColor(const QColor& c)
{
    m_bg = c;
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::goToSpread(int leftPage)
{
    if (!m_cache || m_pageCount <= 0) return;

    leftPage = std::clamp((leftPage / 2) * 2, 0, m_pageCount - 1);
    if (leftPage == m_leftPage && (m_leftPx || m_rightPx)) return;

    m_leftPage = leftPage;
    m_leftPx.reset();
    m_rightPx.reset();

    // Inicia spinner imediatamente — aparece enquanto o render roda
    m_spinnerAngle = 0;
    m_spinnerTimer->start();
    update();

    requestCurrentPages();
    prefetchAdjacentSpreads();
    scheduleFadeIn();
    emit spreadChanged(m_leftPage, m_leftPage + 1);
}

void CasualPdfView::nextSpread() { goToSpread(m_leftPage + 2); }
void CasualPdfView::prevSpread() { goToSpread(m_leftPage - 2); }

// ─────────────────────────────────────────────────────────────────────────────
// requestCurrentPages — solicita as duas páginas do spread atual com
// prioridade ALTA (requestRender usa pool principal do cache).
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::requestCurrentPages()
{
    if (!m_cache) return;

    // Tenta obter do cache sem re-renderizar (hit imediato)
    m_leftPx  = m_cache->get(m_leftPage);
    m_rightPx = m_leftPage + 1 < m_pageCount
                ? m_cache->get(m_leftPage + 1)
                : std::nullopt;

    // Solicita render apenas das que ainda não estão no cache
    if (!m_leftPx)
        m_cache->requestRender(m_leftPage);
    if (!m_rightPx && m_leftPage + 1 < m_pageCount)
        m_cache->requestRender(m_leftPage + 1);

    if (m_leftPx && (m_rightPx || m_leftPage + 1 >= m_pageCount))
        m_spinnerTimer->stop();

    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// prefetchAdjacentSpreads — pré-aquece cache dos spreads vizinhos
// usando requestGalleryRender (pool de baixa prioridade — não compete).
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::prefetchAdjacentSpreads()
{
    if (!m_cache) return;
    const int next = m_leftPage + 2;
    const int prev = m_leftPage - 2;
    if (next < m_pageCount)     m_cache->requestGalleryRender(next);
    if (next + 1 < m_pageCount) m_cache->requestGalleryRender(next + 1);
    if (prev >= 0)              m_cache->requestGalleryRender(prev);
    if (prev + 1 >= 0)          m_cache->requestGalleryRender(prev + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::onPageReady(int page)
{
    if (!m_cache) return;

    if (page == m_leftPage) {
        m_leftPx = m_cache->get(page);
        update();
    } else if (page == m_leftPage + 1) {
        m_rightPx = m_cache->get(page);
        update();
    }

    // Para o spinner assim que ambas as páginas estiverem prontas
    const bool rightDone = m_rightPx.has_value() || (m_leftPage + 1 >= m_pageCount);
    if (m_leftPx && rightDone)
        m_spinnerTimer->stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout helpers
// ─────────────────────────────────────────────────────────────────────────────
QRect CasualPdfView::leftPageRect() const
{
    const int half = (width() - kGutterPx) / 2;
    return QRect(0, 0, half, height());
}

QRect CasualPdfView::rightPageRect() const
{
    const int half = (width() - kGutterPx) / 2;
    return QRect(half + kGutterPx, 0, half, height());
}

QRect CasualPdfView::fitPixmap(const QPixmap& px, const QRect& area) const
{
    const QRect inner = area.adjusted(
        kPageMarginPx, kPageMarginPx, -kPageMarginPx, -kPageMarginPx);
    const QSizeF scaled = px.size().scaled(inner.size(), Qt::KeepAspectRatio);
    const int x = inner.x() + (inner.width()  - (int)scaled.width())  / 2;
    const int y = inner.y() + (inner.height() - (int)scaled.height()) / 2;
    return QRect(QPoint(x, y), scaled.toSize());
}

// ─────────────────────────────────────────────────────────────────────────────
// drawSpinner — arco girante no centro da área de página (estilo iOS)
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::drawSpinner(QPainter& p, const QRect& area) const
{
    const QPoint center = area.center();
    const int r = 14;

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(center);
    p.rotate(m_spinnerAngle);

    // 8 traços, opacidade decrescente
    for (int i = 0; i < 8; ++i) {
        const int alpha = 40 + (215 / 8) * i;
        p.setPen(QPen(QColor(0, 0, 0, alpha), 2.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(0, r / 2 + 2, 0, r);
        p.rotate(45);
    }
    p.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// paintEvent
// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(m_opacity);

    // Fundo
    p.fillRect(rect(), m_bg);

    // Linha central (gutter)
    p.setPen(QPen(QColor(0, 0, 0, 22), 1));
    p.drawLine(width() / 2, kPageMarginPx / 2,
               width() / 2, height() - kPageMarginPx / 2);

    const auto drawPage = [&](const std::optional<QPixmap>& opt,
                               const QRect& area,
                               bool showSpinner) {
        const QRect inner = area.adjusted(
            kPageMarginPx, kPageMarginPx, -kPageMarginPx, -kPageMarginPx);

        // Sombra
        p.fillRect(inner.adjusted(kShadowPx, kShadowPx, kShadowPx, kShadowPx),
                   QColor(0, 0, 0, 28));

        // Fundo branco da página (sempre visível)
        p.fillRect(inner, Qt::white);

        if (opt) {
            const QRect dest = fitPixmap(*opt, area);
            p.drawPixmap(dest, *opt);
        } else if (showSpinner) {
            drawSpinner(p, inner);
        }

        // Borda sutil
        p.setPen(QPen(QColor(0, 0, 0, 16), 1));
        p.drawRect(inner);
    };

    drawPage(m_leftPx, leftPageRect(), /*showSpinner=*/true);

    if (m_leftPage + 1 < m_pageCount)
        drawPage(m_rightPx, rightPageRect(), /*showSpinner=*/true);
    else {
        // Última página: direita em branco sem spinner
        const QRect inner = rightPageRect().adjusted(
            kPageMarginPx, kPageMarginPx, -kPageMarginPx, -kPageMarginPx);
        p.fillRect(inner.adjusted(kShadowPx, kShadowPx, kShadowPx, kShadowPx),
                   QColor(0, 0, 0, 16));
        p.fillRect(inner, Qt::white);
        p.setPen(QPen(QColor(0, 0, 0, 16), 1));
        p.drawRect(inner);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::showEvent(QShowEvent* e)
{
    activateDpi();
    QWidget::showEvent(e);
}

void CasualPdfView::resizeEvent(QResizeEvent*)
{
    // Não muda DPI no resize — kCasualDpi é fixo para spread.
    // Apenas re-solicita se ainda sem cache (mudança de janela).
    if (!m_leftPx || (!m_rightPx && m_leftPage + 1 < m_pageCount))
        requestCurrentPages();
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const int x = e->position().x();
    if      (x < width() / 3)     prevSpread();
    else if (x > width() * 2 / 3) nextSpread();
    setFocus();
}

void CasualPdfView::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Right: case Qt::Key_PageDown: case Qt::Key_Space: nextSpread(); break;
    case Qt::Key_Left:  case Qt::Key_PageUp:                       prevSpread(); break;
    default: QWidget::keyPressEvent(e);
    }
}

void CasualPdfView::wheelEvent(QWheelEvent* e)
{
    if      (e->angleDelta().y() < 0) nextSpread();
    else if (e->angleDelta().y() > 0) prevSpread();
}

// ─────────────────────────────────────────────────────────────────────────────
void CasualPdfView::scheduleFadeIn()
{
    m_fadeAnim->stop();
    m_opacity = 0.0;
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
}
