// casual_sidebar_models.hpp  —  l-reader · Modo Casual
//
// Três QAbstractListModel leves que alimentam as abas do sidebar QML:
//   CasualTocModel       → aba "Sumário"
//   CasualAnnotModel     → aba "Anotações"
//   CasualBookmarkModel  → aba "Marcadores"
//
// Todos expõem os dados via roles nomeados — o QML lê-os como propriedades
// JavaScript, sem nenhum cast ou index mágico.
#pragma once

#include "DocumentEngine/document_engine.hpp"
#include "DocumentEngine/highlight_entry.hpp"
#include "DocumentEngine/pdf_bookmark_manager.hpp"

#include <QAbstractListModel>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// CasualTocModel — sumário / table of contents
//   Roles: title (str), pageOrIndex (int), depth (int), isCurrent (bool)
// ─────────────────────────────────────────────────────────────────────────────
class CasualTocModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role {
    TitleRole = Qt::UserRole + 1,
    PageOrIndexRole,
    DepthRole,
    IsCurrentRole,
  };
  Q_ENUM(Role)

  explicit CasualTocModel(QObject *parent = nullptr);

  void setEntries(const QList<TocEntry> &entries);
  void setCurrentPage(int page); // destaca a entrada activa
  void clear();

  // QAbstractListModel
  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

private:
  QList<TocEntry> m_entries;
  int m_currentPage{-1};
};

// ─────────────────────────────────────────────────────────────────────────────
// CasualAnnotModel — anotações / highlights
//   Roles: annotId (str), text (str), color (str #AARRGGBB), page (int),
//          pageLabel (str "p. N"), snippet (str — primeiros 80 chars do texto)
// ─────────────────────────────────────────────────────────────────────────────
class CasualAnnotModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role {
    AnnotIdRole = Qt::UserRole + 1,
    TextRole,
    ColorRole,
    PageRole,
    PageLabelRole,
    SnippetRole,
  };
  Q_ENUM(Role)

  explicit CasualAnnotModel(QObject *parent = nullptr);

  void setHighlights(const QVector<HighlightEntry> &highlights);
  void clear();

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

signals:
  // Emitido quando o utilizador pede para ir à página da anotação
  void navigateToPage(int page);
  // Emitido quando o utilizador pede para remover a anotação
  void removeAnnotation(QString id);

public slots:
  // Chamados directamente pelo QML via Q_INVOKABLE do controller
  Q_INVOKABLE void requestNavigate(int row);
  Q_INVOKABLE void requestRemove(int row);

private:
  QVector<HighlightEntry> m_highlights;
};

// ─────────────────────────────────────────────────────────────────────────────
// CasualBookmarkModel — marcadores
//   Roles: bmPage (int), bmLabel (str), bmPageLabel (str "p. N")
// ─────────────────────────────────────────────────────────────────────────────
class CasualBookmarkModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Role {
    BmPageRole = Qt::UserRole + 1,
    BmLabelRole,
    BmPageLabelRole,
  };
  Q_ENUM(Role)

  explicit CasualBookmarkModel(QObject *parent = nullptr);

  void setBookmarks(const QList<BookmarkEntry> &bookmarks);
  void clear();

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

signals:
  void navigateToPage(int page);
  void removeBookmark(int page);

public slots:
  Q_INVOKABLE void requestNavigate(int row);
  Q_INVOKABLE void requestRemove(int row);

private:
  QList<BookmarkEntry> m_bookmarks;
};
