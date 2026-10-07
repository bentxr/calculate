#include "icons.hpp"

#include <QImage>

#include <gtest/gtest.h>

namespace {

QImage image(icons::Kind kind, const QColor& ink) {
    return icons::drawn(kind, ink, 32).pixmap(QSize(32, 32)).toImage().convertToFormat(QImage::Format_ARGB32);
}

}  // namespace

TEST(Icons, EveryKindIsLineArtInTheInk) {
    QList<QImage> images;
    for (icons::Kind kind : {icons::Kind::Settings, icons::Kind::Keyboard, icons::Kind::Search}) {
        const QImage drawn = image(kind, Qt::red);
        ASSERT_EQ(drawn.size(), QSize(32, 32));
        int painted = 0;
        for (int y = 0; y < drawn.height(); ++y)
            for (int x = 0; x < drawn.width(); ++x) {
                const QRgb p = drawn.pixel(x, y);
                if (qAlpha(p) == 0) continue;
                ++painted;
                EXPECT_LE(qGreen(p) + qBlue(p), 4) << static_cast<int>(kind);  // only the ink
            }
        EXPECT_GT(painted, 32 * 32 / 20) << static_cast<int>(kind);  // something is drawn…
        EXPECT_LT(painted, 32 * 32 / 2) << static_cast<int>(kind);   // … as lines, not a filled square
        images << drawn;
    }
    EXPECT_NE(images[0], images[1]);  // three different pictures
    EXPECT_NE(images[1], images[2]);
    EXPECT_NE(images[0], images[2]);
}
