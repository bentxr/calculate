#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class QWidget;

// The app's file dialogs: on the desktop the native ones, in the browser a download and the browser's file picker
// (Qt's QFileDialog::saveFileContent and getOpenFileContent serve both). Tests put their own in their place.
namespace filedialogs {

// What an open gives back: the file's name and bytes, or an empty name when it was cancelled.
using Opened = std::function<void(const QString& name, const QByteArray& content)>;
using Save = std::function<void(const QByteArray& content, const QString& suggestedName, QWidget* parent)>;
using Open = std::function<void(const QString& filter, const Opened& opened, QWidget* parent)>;

void save(const QByteArray& content, const QString& suggestedName, QWidget* parent);
void open(const QString& filter, const Opened& opened, QWidget* parent);

// Others in the place of Qt's (nullptr: Qt's again).
void setSave(Save save);
void setOpen(Open open);

}  // namespace filedialogs
