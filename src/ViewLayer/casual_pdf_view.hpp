// casual_pdf_view.hpp  —  l-reader
//
// Widget de leitura "book spread" para o Modo Casual (PDF).
// Otimizações de carregamento:
//   • DPI calculado para o tamanho real do widget (~96 DPI) — renders 2-3x mais rápidos
//   • Pré-renderiza o spread seguinte/anterior em background imediatamente
//   • Mostra spinner animado enquanto as páginas carregam (sem tela branca)
//   • setDpi() chamado SÓ no showEvent — não interfere com o DPI do PdfCanvasView

#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QTimer>
#include <optional>

class PageCache;

class CasualPdfView final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit CasualPdfView(QWidget* parent = nullptr);

    void setPageCache(PageCache* cache, int pageCount);
    void goToSpread(int leftPage);
    void nextSpread();
    void prevSpread();

    [[nodiscard]] int currentLeftPage() const { return m_leftPage;  }
    [[nodiscard]] int pageCount()       const { return m_pageCount; }

    void setBackgroundColor(const QColor& c);

    // Chamado por MainWindow ao entrar/sair do modo casual
    // para ajustar o DPI sem interferir com o PdfCanvasView
    void activateDpi();
    void deactivateDpi();

signals:
    void spreadChanged(int leftPage, int rightPage);

public slots:
    void onPageReady(int page);

protected:
    void paintEvent(QPaintEvent* event)   override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event)     override;
    void mousePressEvent(QMouseEvent* e)  override;
    void keyPressEvent(QKeyEvent* e)      override;
    void wheelEvent(QWheelEvent* e)       override;

private:
    void  requestCurrentPages();
    void  prefetchAdjacentSpreads();
    void  scheduleFadeIn();
    qreal computeDpi() const;

    [[nodiscard]] QRect leftPageRect()  const;
    [[nodiscard]] QRect rightPageRect() const;
    [[nodiscard]] QRect fitPixmap(const QPixmap& px, const QRect& area) const;
    void drawSpinner(QPainter& p, const QRect& area) const;

    qreal opacity()      const { return m_opacity; }
    void  setOpacity(qreal v)  { m_opacity = v; update(); }

    PageCache*  m_cache     = nullptr;
    int         m_pageCount = 0;
    int         m_leftPage  = 0;
    QColor      m_bg        { 0xF5, 0xF5, 0xF5 };

    std::optional<QPixmap> m_leftPx;
    std::optional<QPixmap> m_rightPx;

    qreal               m_opacity      = 1.0;
    QPropertyAnimation* m_fadeAnim     = nullptr;

    // Spinner de carregamento
    QTimer* m_spinnerTimer  = nullptr;
    int     m_spinnerAngle  = 0;

    // Evita chamar setDpi() repetidamente com o mesmo valor
    qreal   m_activeDpi     = 0.0;

    static constexpr int    kGutterPx     = 24;
    static constexpr int    kPageMarginPx = 32;
    static constexpr int    kShadowPx     = 5;
    static constexpr qreal  kCasualDpi    = 110.0;  // fixo para spread — renders rápidos
};
