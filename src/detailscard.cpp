#include "detailscard.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// The info sign, a circle with an "i", drawn rather than typed: the browser build has no font with ⓘ.
class InfoButton : public QToolButton {
public:
    using QToolButton::QToolButton;

    QSize sizeHint() const override {
        const int side = fontMetrics().height() + 6;
        return {side, side};
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QToolButton::paintEvent(event);  // the panel under the mouse
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal d = fontMetrics().height() * 0.8;
        QRectF circle(0, 0, d, d);
        circle.moveCenter(QRectF(rect()).center());
        const QColor ink = palette().color(QPalette::WindowText);
        painter.setPen(QPen(ink, 1.2));
        painter.drawEllipse(circle);
        const qreal stroke = d * 0.13;  // the i: a dot over a stem
        const qreal x = circle.center().x();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawEllipse(QPointF(x, circle.top() + d * 0.28), stroke * 0.6, stroke * 0.6);
        painter.drawRoundedRect(QRectF(x - stroke / 2, circle.top() + d * 0.42, stroke, d * 0.36), stroke / 2, stroke / 2);
    }
};

}  // namespace

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
        auto* info = new InfoButton(rows_);
        info->setObjectName("info:" + row.key);
        info->setToolTip(tr("What does this mean?"));
        info->setAutoRaise(true);
        info->setFocusPolicy(Qt::NoFocus);
        connect(info, &QToolButton::clicked, this, [this, key = row.key, info] { explain(key, info); });
        grid->addWidget(label, line, 0, Qt::AlignTop);
        grid->addWidget(value, line, 1, Qt::AlignTop);
        grid->addWidget(info, line, 2, Qt::AlignTop);
    }
}

// The explanation, about forty characters wide, just under its info sign.
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
