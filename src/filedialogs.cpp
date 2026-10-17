#include "filedialogs.hpp"

#include <QFileDialog>

namespace filedialogs {

namespace {

Save& saver() {
    static Save s;
    return s;
}
Open& opener() {
    static Open o;
    return o;
}

}  // namespace

void save(const QByteArray& content, const QString& suggestedName, QWidget* parent) {
    if (saver()) return saver()(content, suggestedName, parent);
    QFileDialog::saveFileContent(content, suggestedName, parent);
}

void open(const QString& filter, const Opened& opened, QWidget* parent) {
    if (opener()) return opener()(filter, opened, parent);
    QFileDialog::getOpenFileContent(filter, opened, parent);
}

void setSave(Save s) { saver() = std::move(s); }
void setOpen(Open o) { opener() = std::move(o); }

}  // namespace filedialogs
