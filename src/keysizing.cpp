#include "keysizing.hpp"

#include <QFontInfo>
#include <QFontMetrics>
#include <QtGlobal>

namespace {

constexpr double aspect = 0.55;  // height / width

}  // namespace

QSize keySize(QSize screen, QSize reserved, QSize grid, int spacing, QSize minimum) {
    const int across = (screen.width() / 2 - reserved.width() - (grid.width() - 1) * spacing) / grid.width();
    const int down = (screen.height() - reserved.height() - (grid.height() - 1) * spacing) / grid.height();
    const int width = qMax(minimum.width(), qMin(across, qRound(down / aspect)));
    return QSize(width, qMax(minimum.height(), qRound(width * aspect)));
}

KeysLayout keysLayout(QSize screen) { return screen.width() < screen.height() ? KeysLayout::Narrow : KeysLayout::Wide; }

QSize phoneKeySize(QSize screen, int reservedWidth, int columns, int spacing, QSize minimum) {
    const int share = (screen.width() - reservedWidth - (columns - 1) * spacing) / columns;
    const int width = qBound(minimum.width(), share, 2 * minimum.width());
    return QSize(width, qMax(minimum.height(), qRound(width * aspect)));
}

QFont fittedFont(const QFont& font, const QString& label, int width, qreal smallest) {
    if (QFontMetrics(font).horizontalAdvance(label) <= width) return font;
    const int base = QFontInfo(font).pixelSize();
    const int least = qMax(1, qRound(smallest * base));
    QFont fitted = font;
    for (int size = base - 1; size >= least; --size) {
        fitted.setPixelSize(size);
        if (QFontMetrics(fitted).horizontalAdvance(label) <= width) return fitted;
    }
    fitted.setPixelSize(least);
    return fitted;
}
