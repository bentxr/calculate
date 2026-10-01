#include "detailscard.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

DetailsCard::DetailsCard(QWidget* parent) : QFrame(parent, Qt::Popup) {
    setFrameShape(QFrame::StyledPanel);
    layout_ = new QVBoxLayout(this);
    tip_ = new QFrame(this, Qt::Popup);
    tip_->setObjectName("explanation");
    tip_->setFrameShape(QFrame::StyledPanel);
    tipText_ = new QLabel(tip_);
    tipText_->setWordWrap(true);
    (new QVBoxLayout(tip_))->addWidget(tipText_);
}

void DetailsCard::setRows(const QList<view::DetailRow>& rows) {
    delete rows_;
    rows_ = new QWidget(this);
    layout_->addWidget(rows_);
    auto* grid = new QGridLayout(rows_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);
    for (int line = 0; line < rows.size(); ++line) {
        const view::DetailRow& row = rows[line];
        auto* label = new QLabel(row.label, rows_);
        label->setObjectName("label:" + row.key);
        QFont bold = label->font();
        bold.setBold(true);
        label->setFont(bold);
        auto* value = new QLabel(row.value, rows_);
        value->setObjectName("value:" + row.key);
        value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* info = new QToolButton(rows_);
        info->setObjectName("info:" + row.key);
        info->setText(QStringLiteral("ⓘ"));
        info->setToolTip(tr("What does this mean?"));
        info->setAutoRaise(true);
        info->setFocusPolicy(Qt::NoFocus);
        connect(info, &QToolButton::clicked, this, [this, key = row.key, info] { explain(key, info); });
        grid->addWidget(label, line, 0, Qt::AlignTop);
        grid->addWidget(value, line, 1, Qt::AlignTop);
        grid->addWidget(info, line, 2, Qt::AlignTop);
    }
}

// The explanation, about forty characters wide, just under its ⓘ.
void DetailsCard::explain(const QString& key, QToolButton* info) {
    tipText_->setText(view::explanation(key));
    tip_->setFixedWidth(fontMetrics().averageCharWidth() * 40);
    tip_->adjustSize();
    tip_->move(info->mapToGlobal(QPoint(0, info->height())));
    tip_->show();
}

void DetailsCard::popUp(QWidget* under) {
    setFixedWidth(under->width());
    move(under->mapToGlobal(QPoint(0, under->height())));
    resize(width(), sizeHint().height());
    show();
}
