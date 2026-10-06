#include "webclipboard.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace webclipboard {

namespace {

// The request waiting for the browser's answer (a newer one replaces it).
struct Pending {
    QPointer<QObject> receiver;
    std::function<void(const QString&)> done;
    std::function<void()> refused;
};

Pending& pending() {
    static Pending p;
    return p;
}

void deliver(const std::function<void()>& call) {
    QObject* receiver = pending().receiver;
    if (receiver) QMetaObject::invokeMethod(receiver, call, Qt::QueuedConnection);
}

void resolved(const std::string& text) {
    const auto done = pending().done;
    deliver([done, text] { done(QString::fromStdString(text)); });
}

void rejected(emscripten::val) { deliver(pending().refused); }

}  // namespace

void readText(QObject* receiver, std::function<void(const QString&)> done, std::function<void()> refused) {
    pending() = Pending{receiver, std::move(done), std::move(refused)};
    const emscripten::val clipboard = emscripten::val::global("navigator")["clipboard"];
    if (clipboard.isUndefined() || clipboard["readText"].isUndefined()) {  // an insecure page or an older browser
        deliver(pending().refused);
        return;
    }
    clipboard.call<emscripten::val>("readText").call<emscripten::val>("then", emscripten::val::module_property("calculateClipboardText"),
                                                                       emscripten::val::module_property("calculateClipboardRefused"));
}

}  // namespace webclipboard

EMSCRIPTEN_BINDINGS(calculate_clipboard) {
    emscripten::function("calculateClipboardText", &webclipboard::resolved);
    emscripten::function("calculateClipboardRefused", &webclipboard::rejected);
}
