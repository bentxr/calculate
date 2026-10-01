#include "keysizing.hpp"

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
