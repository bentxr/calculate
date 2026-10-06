#include "settingsfile.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

namespace settingsfile {

namespace {

constexpr int version = 1;

}  // namespace

QByteArray write(const QMap<QString, QString>& current, const QMap<QString, QString>& defaults) {
    QJsonObject settings;  // its keys come out sorted
    for (auto it = current.cbegin(); it != current.cend(); ++it)
        if (defaults.value(it.key()) != it.value()) settings.insert(it.key(), it.value());
    const QJsonObject file{{"version", version}, {"settings", settings}};
    return QJsonDocument(file).toJson(QJsonDocument::Indented);
}

Read read(const QByteArray& file, const QMap<QString, QStringList>& allowed) {
    Read r;
    const QJsonDocument document = QJsonDocument::fromJson(file);
    const QJsonObject root = document.object();
    if (!document.isObject() || !root.value("version").isDouble() || !root.value("settings").isObject()) {
        r.problems << QCoreApplication::translate("settingsfile", "This is not a settings file of this app");
        return r;
    }
    if (root.value("version").toInt() != version) {
        r.problems << (root.value("version").toInt() > version
                           ? QCoreApplication::translate("settingsfile", "The file was written by a newer version")
                           : QCoreApplication::translate("settingsfile", "This is not a settings file of this app"));
        return r;
    }
    const QJsonObject settings = root.value("settings").toObject();
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        const QString value = it.value().toString();
        if (!allowed.contains(it.key()))
            r.problems << QCoreApplication::translate("settingsfile", "unknown setting “%1”").arg(it.key());
        else if (!allowed.value(it.key()).contains(value))
            r.problems << QCoreApplication::translate("settingsfile", "“%1” is not a value of “%2”").arg(value, it.key());
        else
            r.values.insert(it.key(), value);
    }
    return r;
}

}  // namespace settingsfile
