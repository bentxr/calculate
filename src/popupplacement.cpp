#include "popupplacement.hpp"

#include <QEvent>
#include <QLabel>
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

PopupBounds::PopupBounds(QWidget* window) : QObject(window), window_(window) {}

bool PopupBounds::eventFilter(QObject* watched, QEvent* event) {
    // On showing, and whenever one that shows is placed again (the browser places a menu's list again
    // after showing it). Putting it back inside causes one more Move or Resize, which then changes nothing.
    const QEvent::Type when = event->type();
    if (!watched->isWidgetType() || (when != QEvent::Show && when != QEvent::Move && when != QEvent::Resize)) return false;
    auto* popup = static_cast<QWidget*>(watched);
    if (when != QEvent::Show && !popup->isVisible()) return false;
    const Qt::WindowType type = popup->windowType();
    if (type != Qt::Popup && type != Qt::ToolTip && type != Qt::Dialog) return false;
    const QRect bounds = window_->geometry();
    if (auto* label = qobject_cast<QLabel*>(popup); label && label->width() > bounds.width()) {
        label->setWordWrap(true);
        label->resize(bounds.width(), label->heightForWidth(bounds.width()));
    }
    const QRect inside = keptInside(popup->geometry(), bounds);
    if (inside != popup->geometry()) popup->setGeometry(inside);
    return false;
}
