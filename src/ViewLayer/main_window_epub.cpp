// main_window_epub.cpp  —  l-reader
//
// Responsabilidades:
//   • Navegação de capítulos EPUB (loadEpubChapter)
//   • Injeção de CSS customizado no QWebEngineView (injectEpubCSS)
//   • Sincronização de estado ao mudar de URL (onWebUrlChanged)
//   • wireEpubSignals()

#include "main_window.hpp"
#include "Ui/epub_style.hpp"

#include <QWebEngineView>
#include <algorithm>

using namespace Qt::Literals::StringLiterals;

// ─────────────────────────────────────────────────────────────────────────────
// wireEpubSignals
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::wireEpubSignals()
{
    connect(m_webView, &QWebEngineView::loadFinished,
            this, &MainWindow::onWebLoadFinished);
    connect(m_webView, &QWebEngineView::urlChanged,
            this, &MainWindow::onWebUrlChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadEpubChapter — carrega o índice indicado da spine no QWebEngineView
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

// ─────────────────────────────────────────────────────────────────────────────
// onWebLoadFinished — injeta CSS e rola para âncora pendente
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
// onWebUrlChanged — sincroniza m_currentChapter quando a URL muda externamente
// ─────────────────────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────────────────────
// injectEpubCSS — injeta/substitui a tag <style id="lreader-css"> no DOM
// ─────────────────────────────────────────────────────────────────────────────
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
