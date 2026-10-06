#include "longpress.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>

LongPress::LongPress(QWidget* widget) : QObject(widget) {
    timer_.setSingleShot(true);
    timer_.setInterval(delay);
    connect(&timer_, &QTimer::timeout, this, [this] {
        swallowRelease_ = true;
        emit longPressed(pressedAt_);
    });
    widget->installEventFilter(this);
}

bool LongPress::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        // A menu opened by a long press may take its release; the next press starts afresh.
        swallowRelease_ = false;
        if (mouse->button() == Qt::LeftButton) {
            pressedAt_ = mouse->position().toPoint();
            timer_.start();
        }
        break;
    }
    case QEvent::MouseMove:
        if (timer_.isActive()
            && (static_cast<QMouseEvent*>(event)->position().toPoint() - pressedAt_).manhattanLength() > QApplication::startDragDistance())
            timer_.stop();  // a drag or a scroll, not a long press
        break;
    case QEvent::MouseButtonRelease:
        timer_.stop();
        if (swallowRelease_) {
            swallowRelease_ = false;
            return true;
        }
        break;
    default: break;
    }
    return QObject::eventFilter(watched, event);
}
