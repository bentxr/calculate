#include "popupplacement.hpp"

#include <QWidget>

QRect placed(QSize size, QRect anchor, QRect bounds) {
    const int width = qMin(size.width(), bounds.width());
    const int x = qBound(bounds.left(), anchor.left(), bounds.left() + bounds.width() - width);
    const int under = bounds.top() + bounds.height() - (anchor.top() + anchor.height());  // rows below the anchor
    const int over = anchor.top() - bounds.top();                                           // rows above it
    if (size.height() <= under || (size.height() > over && under >= over)) {
        return QRect(x, anchor.top() + anchor.height(), width, qMin(size.height(), under));
    }
    const int height = qMin(size.height(), over);
    return QRect(x, anchor.top() - height, width, height);
}

QRect keptInside(QRect rect, QRect bounds) {
    const QSize size = rect.size().boundedTo(bounds.size());
    const int x = qBound(bounds.left(), rect.left(), bounds.left() + bounds.width() - size.width());
    const int y = qBound(bounds.top(), rect.top(), bounds.top() + bounds.height() - size.height());
    return QRect(QPoint(x, y), size);
}

QRect globalGeometry(const QWidget* widget) { return QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size()); }

QRect popupBounds(const QWidget* widget) {
    const QWidget* window = widget->window();
    while (window->parentWidget()) window = window->parentWidget()->window();
    return window->geometry();
}
