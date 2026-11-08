#include "devicekeyboard.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodQueryEvent>

DeviceKeyboard::DeviceKeyboard(QObject* parent) : QObject(parent) { QCoreApplication::instance()->installEventFilter(this); }

void DeviceKeyboard::setAllowed(bool allowed) {
    allowed_ = allowed;
    QInputMethod* method = QGuiApplication::inputMethod();
    method->update(Qt::ImEnabled);  // the platform asks the focused box again, and acts on its answer
    if (allowed) method->show();
    else method->hide();
}

// Every box is asked whether it takes an input method through this event: while the keyboard is not
// allowed the answer is no, and the box is not asked.
bool DeviceKeyboard::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::InputMethodQuery || allowed_) return QObject::eventFilter(watched, event);
    static_cast<QInputMethodQueryEvent*>(event)->setValue(Qt::ImEnabled, false);
    event->accept();
    return true;
}
