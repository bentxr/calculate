#pragma once

#include <QByteArray>
#include <QMap>
#include <QSet>
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
// A key in `lists` holds a space-separated list of its allowed values, each at most once, in any order (an empty value
// is an empty list).
Read read(const QByteArray& file, const QMap<QString, QStringList>& allowed, const QSet<QString>& lists = {});

}  // namespace settingsfile
