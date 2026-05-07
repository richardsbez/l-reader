// bookmark_item_widget.cpp
#include "bookmark_item_widget.hpp"

#include <QMouseEvent>
#include <QKeyEvent>

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
BookmarkItemWidget::BookmarkItemWidget(int page, const QString& label,
                                       QWidget* parent)
    : QWidget(parent)
    , m_page(page)
    , m_label(label)
{
    buildUi();
    setLabel(label);

    // Cursor de mão indica clicabilidade
    setCursor(Qt::PointingHandCursor);
}

// ─────────────────────────────────────────────────────────────────────────────
// buildUi
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::buildUi()
{
    setObjectName(QStringLiteral("bookmarkItem"));
    // Habilita rastreamento de hover para que o seletor CSS :hover funcione
    setAttribute(Qt::WA_Hover, true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 5, 6, 5);
    layout->setSpacing(6);

    // ── Ícone + número de página ──────────────────────────────────────────
    m_pageLabel = new QLabel(this);
    m_pageLabel->setObjectName(QStringLiteral("bookmarkPageLabel"));
    m_pageLabel->setText(tr("p.%1").arg(m_page + 1));
    m_pageLabel->setFixedWidth(38);
    m_pageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // ── Stack: label display / edit ───────────────────────────────────────
    m_labelStack = new QStackedWidget(this);
    m_labelStack->setObjectName(QStringLiteral("bookmarkLabelStack"));

    m_displayLbl = new QLabel(m_labelStack);
    m_displayLbl->setObjectName(QStringLiteral("bookmarkDisplayLabel"));
    m_displayLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_displayLbl->setWordWrap(false);
    // Duplo-clique → edição inline
    m_displayLbl->installEventFilter(this);

    m_editLine = new QLineEdit(m_labelStack);
    m_editLine->setObjectName(QStringLiteral("bookmarkEditLine"));
    m_editLine->setPlaceholderText(tr("Nome do marcador..."));
    m_editLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(m_editLine, &QLineEdit::returnPressed, this, &BookmarkItemWidget::commitEdit);
    // Instala filtro para capturar Escape
    m_editLine->installEventFilter(this);

    m_labelStack->addWidget(m_displayLbl);   // index 0 — display
    m_labelStack->addWidget(m_editLine);     // index 1 — edit
    m_labelStack->setCurrentIndex(0);

    // ── Botão remover ─────────────────────────────────────────────────────
    m_removeBtn = new QToolButton(this);
    m_removeBtn->setObjectName(QStringLiteral("bookmarkRemoveBtn"));
    m_removeBtn->setText(QStringLiteral(" "));
    m_removeBtn->setToolTip(tr("Remover marcador"));
    m_removeBtn->setFixedSize(22, 22);
    m_removeBtn->setCursor(Qt::ArrowCursor);
    connect(m_removeBtn, &QToolButton::clicked,
            this, [this] { emit removeRequested(m_page); });

    layout->addWidget(m_pageLabel);
    layout->addWidget(m_labelStack, 1);
    layout->addWidget(m_removeBtn);
}

// ─────────────────────────────────────────────────────────────────────────────
// setLabel
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::setLabel(const QString& label)
{
    m_label = label;
    if (m_displayLbl) {
        // Elide text se muito longo
        const QFontMetrics fm(m_displayLbl->font());
        const int maxW = m_labelStack->width() > 20
                             ? m_labelStack->width() - 4 : 120;
        m_displayLbl->setText(
            fm.elidedText(label, Qt::ElideRight, maxW));
        m_displayLbl->setToolTip(label);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// startEditing — ativa o campo de edição inline
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::startEditing()
{
    if (!m_editLine || !m_labelStack) return;
    m_editLine->setText(m_label);
    m_editLine->selectAll();
    m_labelStack->setCurrentIndex(1);
    m_editLine->setFocus(Qt::MouseFocusReason);
}

// ─────────────────────────────────────────────────────────────────────────────
// commitEdit — confirma a edição e emite renameRequested
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::commitEdit()
{
    const QString newLabel = m_editLine->text().trimmed();
    m_labelStack->setCurrentIndex(0);

    if (!newLabel.isEmpty() && newLabel != m_label) {
        m_label = newLabel;
        setLabel(m_label);
        emit renameRequested(m_page, m_label);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// cancelEdit — descarta a edição
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::cancelEdit()
{
    m_labelStack->setCurrentIndex(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent — navega ao clicar no item
// ─────────────────────────────────────────────────────────────────────────────
void BookmarkItemWidget::mousePressEvent(QMouseEvent* event)
{
    // Não dispara navegação se o clique foi no botão remover ou no campo edit
    if (m_removeBtn && m_removeBtn->underMouse()) return;
    if (m_labelStack && m_labelStack->currentIndex() == 1) return;

    if (event->button() == Qt::LeftButton)
        emit navigateRequested(m_page);

    QWidget::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// eventFilter — duplo-clique no label e Escape no campo de edição
// ─────────────────────────────────────────────────────────────────────────────
bool BookmarkItemWidget::eventFilter(QObject* obj, QEvent* event)
{
    // Duplo-clique no rótulo → inicia edição
    if (obj == m_displayLbl && event->type() == QEvent::MouseButtonDblClick) {
        startEditing();
        return true;
    }

    // Escape no campo de edição → cancela
    if (obj == m_editLine && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            cancelEdit();
            return true;
        }
    }

    // Perda de foco no campo de edição → confirma
    if (obj == m_editLine && event->type() == QEvent::FocusOut) {
        commitEdit();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}
