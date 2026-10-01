#include "detailscard.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

DetailsCard::DetailsCard(QWidget* parent) : QFrame(parent, Qt::Popup) {
    setFrameShape(QFrame::StyledPanel);
    new QGridLayout(this);
}

void DetailsCard::setRows(const QList<view::DetailRow>& rows) {
    delete layout();
    qDeleteAll(findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    auto* grid = new QGridLayout(this);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(2);  // hidden explanation rows would otherwise double the gaps
    int line = 0;
    for (const view::DetailRow& row : rows) {
        auto* label = new QLabel(row.label, this);
        label->setObjectName("label:" + row.key);
        QFont bold = label->font();
        bold.setBold(true);
        label->setFont(bold);
        auto* value = new QLabel(row.value, this);
        value->setObjectName("value:" + row.key);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* info = new QToolButton(this);
        info->setObjectName("info:" + row.key);
        info->setText(QStringLiteral("ⓘ"));
        info->setToolTip(tr("What does this mean?"));
        info->setCheckable(true);
        info->setAutoRaise(true);
        info->setFocusPolicy(Qt::NoFocus);
        auto* explanation = new QLabel(view::explanation(row.key), this);
        explanation->setObjectName("explanation:" + row.key);
        explanation->setWordWrap(true);
        explanation->setForegroundRole(QPalette::PlaceholderText);
        explanation->hide();
        connect(info, &QToolButton::toggled, this, [this, explanation](bool on) {
            explanation->setVisible(on);
            resize(width(), sizeHint().height());
        });
        grid->addWidget(label, line, 0, Qt::AlignTop);
        grid->addWidget(value, line, 1, Qt::AlignTop);
        grid->addWidget(info, line, 2, Qt::AlignTop);
        grid->addWidget(explanation, line + 1, 0, 1, 3);
        line += 2;
    }
}

void DetailsCard::popUp(QWidget* under) {
    setFixedWidth(under->width());
    move(under->mapToGlobal(QPoint(0, under->height())));
    resize(width(), sizeHint().height());
    show();
}
