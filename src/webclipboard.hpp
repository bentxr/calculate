#pragma once

#include <QString>

#include <functional>

class QObject;

// The browser's clipboard, which a page can only read asynchronously (WebAssembly builds only).
namespace webclipboard {

// Asks the browser for the clipboard's text: `done` gets it, or `refused` runs (no permission, an older
// browser). Either one is called later, through `receiver`'s event loop.
void readText(QObject* receiver, std::function<void(const QString&)> done, std::function<void()> refused);

}  // namespace webclipboard
