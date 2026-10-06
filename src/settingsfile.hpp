#pragma once

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

// The settings as a small JSON file: only the ones that differ from the defaults, so a file stays meaningful when
// defaults change, and every value is checked on the way back in.
namespace settingsfile {

QByteArray write(const QMap<QString, QString>& current, const QMap<QString, QString>& defaults);

struct Read {
    QMap<QString, QString> values;  // the keys and values that can be applied
    QStringList problems;           // one line per thing that could not be used, in the current language
};
Read read(const QByteArray& file, const QMap<QString, QStringList>& allowed);

}  // namespace settingsfile
