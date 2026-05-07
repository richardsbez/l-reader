// bookmark_item_widget.hpp
// ─────────────────────────────────────────────────────────────────────────────
// BookmarkItemWidget — um item editável na lista de marcadores.
//
// Layout visual:
//  ┌───────────────────────────────────────────────┐
//  │  🔖  [página]  [──  label editável  ──]  [✕]  │
//  └───────────────────────────────────────────────┘
//
// • Clique no item (fora do botão) → navega para a página
// • Duplo-clique no label → abre edição inline (QLineEdit)
// • Enter / perda de foco  → confirma renomeação
// • Escape                 → cancela
// • Botão ✕                → remove marcador
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QHBoxLayout>
#include <QStackedWidget>

class BookmarkItemWidget final : public QWidget {
    Q_OBJECT
public:
    explicit BookmarkItemWidget(int page, const QString& label,
                                QWidget* parent = nullptr);

    [[nodiscard]] int     page()  const { return m_page;  }
    [[nodiscard]] QString label() const { return m_label; }

    void setLabel(const QString& label);
    void startEditing();   // ativa modo de edição inline

signals:
    void navigateRequested(int page);
    void renameRequested(int page, QString label);
    void removeRequested(int page);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void commitEdit();
    void cancelEdit();

private:
    void buildUi();

    int     m_page  = -1;
    QString m_label;

    // ── Widgets ───────────────────────────────────────────────────────────
    QLabel*        m_pageLabel   = nullptr;
    QStackedWidget* m_labelStack = nullptr;   // 0=display, 1=edit
    QLabel*        m_displayLbl  = nullptr;
    QLineEdit*     m_editLine    = nullptr;
    QToolButton*   m_removeBtn   = nullptr;
};
