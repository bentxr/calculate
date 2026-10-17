#include "plotfonts.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QDir>
#include <QFile>
#include <QMap>

#include <mutex>

namespace {

// Every embedded font by file name, read on the first call and never changed after: the drawing thread reads the
// bytes while the window lives.
const QMap<QString, QByteArray>& fonts() {
    static const QMap<QString, QByteArray> all = [] {
        QMap<QString, QByteArray> read;
        const QDir dir(QStringLiteral(":/plotfonts"));
        for (const QString& name : dir.entryList(QDir::Files)) {
            QFile file(dir.filePath(name));
            if (file.open(QIODevice::ReadOnly)) read.insert(name, file.readAll());
        }
        return read;
    }();
    return all;
}

}  // namespace

const QByteArray* plotFontBytes(const QString& fileName) {
    const auto& all = fonts();
    const auto found = all.constFind(fileName);
    return found == all.constEnd() ? nullptr : &found.value();
}

void ensurePlotFonts() {
    static std::once_flag once;
    std::call_once(once, installPlotFonts);
}

void installPlotFonts() {
    fonts();  // read them now, not in the middle of a drawing
    calculate_plot::useFontBytes([](std::string_view fileName) -> std::string_view {
        const QByteArray* bytes = plotFontBytes(QString::fromUtf8(fileName.data(), static_cast<qsizetype>(fileName.size())));
        return bytes ? std::string_view(bytes->constData(), static_cast<std::size_t>(bytes->size())) : std::string_view();
    });
}
