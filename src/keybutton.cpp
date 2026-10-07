#include "keybutton.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

KeyButton::KeyButton(QWidget* parent) : QPushButton(parent) {
    hold_.setSingleShot(true);
    hold_.setInterval(longPressMs);
    connect(&hold_, &QTimer::timeout, this, [this] {
        offered_ = true;
        setDown(false);
        emit moreRequested();
    });
}

void KeyButton::setHasMore(bool more) {
    hasMore_ = more;
    update();
}

void KeyButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        if (hasMore_) {
            emit moreRequested();
            event->accept();
        } else {
            event->ignore();
        }
        return;
    }
    if (event->button() == Qt::LeftButton && hasMore_) hold_.start();
    offered_ = false;
    QPushButton::mousePressEvent(event);
}

void KeyButton::mouseReleaseEvent(QMouseEvent* event) {
    hold_.stop();
    if (offered_) {
        offered_ = false;
        event->accept();
        return;
    }
    QPushButton::mouseReleaseEvent(event);
}

// The key as usual, and for a key with more faces a small triangle in its top-right corner.
void KeyButton::paintEvent(QPaintEvent* event) {
    QPushButton::paintEvent(event);
    if (!hasMore_) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::ButtonText));
    const qreal side = height() / 5.0, right = width() - 3, top = 3;
    painter.drawPolygon(QPolygonF({QPointF(right - side, top), QPointF(right, top), QPointF(right, top + side)}));
}
