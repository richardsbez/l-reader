// casual_mode_widget.hpp  —  l-reader · Modo Casual
//
// Widget de integração: hospeda o motor QML dentro da arquitectura QWidget
// via QQuickWidget.
//
// v2 — expõe os três modelos de sidebar ao QML:
//   casualTocModel      → aba Sumário
//   casualAnnotModel    → aba Anotações
//   casualBookmarkModel → aba Marcadores
//
// Os modelos são alimentados externamente pela MainWindow via:
//   widget->setTocEntries(entries)
//   widget->setHighlights(highlights)
//   widget->setBookmarks(bookmarks)
//   widget->setCurrentPage(page)      // actualiza highlight de TOC
#pragma once

#include <QQuickWidget>
#include <QUrl>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>

#include "casual_mode_controller.hpp"
#include "casual_sidebar_models.hpp"

class CasualModeWidget final : public QQuickWidget {
  Q_OBJECT

public:
  explicit CasualModeWidget(QWidget *parent = nullptr)
      : QQuickWidget(parent), m_controller(new CasualModeController(this)),
        m_tocModel(new CasualTocModel(this)),
        m_annotModel(new CasualAnnotModel(this)),
        m_bookmarkModel(new CasualBookmarkModel(this)) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    auto *ctx = rootContext();
    ctx->setContextProperty(QStringLiteral("casualCtrl"), m_controller);
    ctx->setContextProperty(QStringLiteral("tocListModel"), m_tocModel);
    ctx->setContextProperty(QStringLiteral("annotListModel"), m_annotModel);
    ctx->setContextProperty(QStringLiteral("bookmarkListModel"),
                            m_bookmarkModel);

    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
  }

  [[nodiscard]] CasualModeController *controller() const noexcept {
    return m_controller;
  }
  [[nodiscard]] CasualTocModel *tocModel() const noexcept { return m_tocModel; }
  [[nodiscard]] CasualAnnotModel *annotModel() const noexcept {
    return m_annotModel;
  }
  [[nodiscard]] CasualBookmarkModel *bookmarkModel() const noexcept {
    return m_bookmarkModel;
  }

  // ── Alimentação de dados — chamados pela MainWindow ───────────────────
  void setTocEntries(const QList<TocEntry> &entries) {
    m_tocModel->setEntries(entries);
  }
  void setHighlights(const QVector<HighlightEntry> &highlights) {
    m_annotModel->setHighlights(highlights);
  }
  void setBookmarks(const QList<BookmarkEntry> &bookmarks) {
    m_bookmarkModel->setBookmarks(bookmarks);
  }
  void setCurrentPage(int page) { m_tocModel->setCurrentPage(page); }
  void clearSidebarData() {
    m_tocModel->clear();
    m_annotModel->clear();
    m_bookmarkModel->clear();
  }

protected:
  void showEvent(QShowEvent *event) override {
    if (!m_qmlLoaded) {
      m_qmlLoaded = true;
      setSource(QUrl(
          QStringLiteral("qrc:/LReader/LReader/Casual/casual_mode_view.qml")));
    }
    QQuickWidget::showEvent(event);
  }

private:
  CasualModeController *const m_controller;
  CasualTocModel *const m_tocModel;
  CasualAnnotModel *const m_annotModel;
  CasualBookmarkModel *const m_bookmarkModel;
  bool m_qmlLoaded = false;
};
