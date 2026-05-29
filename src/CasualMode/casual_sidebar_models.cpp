// casual_sidebar_models.cpp  —  l-reader · Modo Casual
#include "casual_sidebar_models.hpp"

#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// CasualTocModel
// ─────────────────────────────────────────────────────────────────────────────
CasualTocModel::CasualTocModel(QObject *parent) : QAbstractListModel(parent) {}

void CasualTocModel::setEntries(const QList<TocEntry> &entries) {
  beginResetModel();
  m_entries = entries;
  endResetModel();
}

void CasualTocModel::setCurrentPage(int page) {
  if (m_currentPage == page)
    return;
  m_currentPage = page;
  // Notifica somente o role isCurrent (optimização)
  if (!m_entries.isEmpty())
    emit dataChanged(index(0), index(m_entries.size() - 1), {IsCurrentRole});
}

void CasualTocModel::clear() {
  beginResetModel();
  m_entries.clear();
  m_currentPage = -1;
  endResetModel();
}

int CasualTocModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant CasualTocModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_entries.size())
    return {};
  const TocEntry &e = m_entries.at(index.row());
  switch (role) {
  case TitleRole:
    return e.title;
  case PageOrIndexRole:
    return e.page;
  case DepthRole:
    return e.depth;
  case IsCurrentRole: {
    // "activo" se esta é a entrada de TOC mais recente com page <=
    // m_currentPage
    if (m_currentPage < 0 || e.page < 0)
      return false;
    if (e.page > m_currentPage)
      return false;
    // Verifica se não há outra entrada mais próxima abaixo desta
    for (int i = index.row() + 1; i < m_entries.size(); ++i) {
      const int p = m_entries.at(i).page;
      if (p >= 0 && p <= m_currentPage)
        return false;
    }
    return true;
  }
  default:
    return {};
  }
}

QHash<int, QByteArray> CasualTocModel::roleNames() const {
  return {
      {TitleRole, "title"},
      {PageOrIndexRole, "pageOrIndex"},
      {DepthRole, "depth"},
      {IsCurrentRole, "isCurrent"},
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// CasualAnnotModel
// ─────────────────────────────────────────────────────────────────────────────
CasualAnnotModel::CasualAnnotModel(QObject *parent)
    : QAbstractListModel(parent) {}

void CasualAnnotModel::setHighlights(
    const QVector<HighlightEntry> &highlights) {
  beginResetModel();
  m_highlights = highlights;
  // Ordena por página da primeira span
  std::sort(m_highlights.begin(), m_highlights.end(),
            [](const HighlightEntry &a, const HighlightEntry &b) {
              const int pa = a.spans.isEmpty() ? -1 : a.spans.first().page;
              const int pb = b.spans.isEmpty() ? -1 : b.spans.first().page;
              return pa < pb;
            });
  endResetModel();
}

void CasualAnnotModel::clear() {
  beginResetModel();
  m_highlights.clear();
  endResetModel();
}

int CasualAnnotModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_highlights.size());
}

QVariant CasualAnnotModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_highlights.size())
    return {};
  const HighlightEntry &h = m_highlights.at(index.row());
  switch (role) {
  case AnnotIdRole:
    return h.id;
  case TextRole:
    return h.text;
  case ColorRole:
    return h.color.name(QColor::HexArgb);
  case PageRole:
    return h.spans.isEmpty() ? -1 : h.spans.first().page;
  case PageLabelRole: {
    const int pg = h.spans.isEmpty() ? -1 : h.spans.first().page;
    return pg >= 0 ? QStringLiteral("p. %1").arg(pg + 1) : QString{};
  }
  case SnippetRole: {
    const QString t = h.text.trimmed();
    return t.length() > 80 ? t.left(77) + QStringLiteral("…") : t;
  }
  default:
    return {};
  }
}

QHash<int, QByteArray> CasualAnnotModel::roleNames() const {
  return {
      {AnnotIdRole, "annotId"},          {TextRole, "annotText"},
      {ColorRole, "annotColor"},         {PageRole, "annotPage"},
      {PageLabelRole, "annotPageLabel"}, {SnippetRole, "annotSnippet"},
  };
}

void CasualAnnotModel::requestNavigate(int row) {
  if (row < 0 || row >= m_highlights.size())
    return;
  const int pg = m_highlights.at(row).spans.isEmpty()
                     ? -1
                     : m_highlights.at(row).spans.first().page;
  if (pg >= 0)
    emit navigateToPage(pg);
}

void CasualAnnotModel::requestRemove(int row) {
  if (row < 0 || row >= m_highlights.size())
    return;
  emit removeAnnotation(m_highlights.at(row).id);
}

// ─────────────────────────────────────────────────────────────────────────────
// CasualBookmarkModel
// ─────────────────────────────────────────────────────────────────────────────
CasualBookmarkModel::CasualBookmarkModel(QObject *parent)
    : QAbstractListModel(parent) {}

void CasualBookmarkModel::setBookmarks(const QList<BookmarkEntry> &bookmarks) {
  beginResetModel();
  m_bookmarks = bookmarks;
  endResetModel();
}

void CasualBookmarkModel::clear() {
  beginResetModel();
  m_bookmarks.clear();
  endResetModel();
}

int CasualBookmarkModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_bookmarks.size());
}

QVariant CasualBookmarkModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_bookmarks.size())
    return {};
  const BookmarkEntry &b = m_bookmarks.at(index.row());
  switch (role) {
  case BmPageRole:
    return b.page;
  case BmLabelRole:
    return b.label.isEmpty() ? QStringLiteral("p. %1").arg(b.page + 1)
                             : b.label;
  case BmPageLabelRole:
    return QStringLiteral("p. %1").arg(b.page + 1);
  default:
    return {};
  }
}

QHash<int, QByteArray> CasualBookmarkModel::roleNames() const {
  return {
      {BmPageRole, "bmPage"},
      {BmLabelRole, "bmLabel"},
      {BmPageLabelRole, "bmPageLabel"},
  };
}

void CasualBookmarkModel::requestNavigate(int row) {
  if (row < 0 || row >= m_bookmarks.size())
    return;
  emit navigateToPage(m_bookmarks.at(row).page);
}

void CasualBookmarkModel::requestRemove(int row) {
  if (row < 0 || row >= m_bookmarks.size())
    return;
  emit removeBookmark(m_bookmarks.at(row).page);
}
