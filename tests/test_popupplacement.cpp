#include "popupplacement.hpp"

#include "printers.hpp"

#include <QFrame>
#include <QLabel>

#include <gtest/gtest.h>

namespace {

const QRect window(0, 0, 800, 600);

}  // namespace

TEST(PopupPlacement, JustUnderTheAnchorWhenItFits) {
    EXPECT_EQ(placed(QSize(200, 100), QRect(50, 50, 100, 20), window), QRect(50, 70, 200, 100));
}

TEST(PopupPlacement, OverTheAnchorWhenOnlyThatSideHasRoom) {
    // under: 600 − 540 = 60 rows, too few; over: 520.
    EXPECT_EQ(placed(QSize(200, 100), QRect(50, 520, 100, 20), window), QRect(50, 420, 200, 100));
}

TEST(PopupPlacement, MovesLeftToStayInside) {
    EXPECT_EQ(placed(QSize(200, 100), QRect(700, 50, 40, 20), window), QRect(600, 70, 200, 100));
}

TEST(PopupPlacement, CutToTheRoomierSideWhenNeitherFits) {
    // under 600 − 270 = 330, over 250: under, cut to 330.
    EXPECT_EQ(placed(QSize(300, 400), QRect(0, 250, 100, 20), window), QRect(0, 270, 300, 330));
    // under 600 − 370 = 230, over 350: over, cut to 350.
    EXPECT_EQ(placed(QSize(300, 400), QRect(0, 350, 100, 20), window), QRect(0, 0, 300, 350));
}

TEST(PopupPlacement, NeverWiderThanTheWindow) {
    EXPECT_EQ(placed(QSize(1000, 100), QRect(50, 50, 100, 20), window), QRect(0, 70, 800, 100));
}

TEST(PopupPlacement, AWindowAwayFromTheOrigin) {
    // bounds 100…500 across: x = min(450, 500 − 100) = 400.
    EXPECT_EQ(placed(QSize(100, 50), QRect(450, 150, 20, 20), QRect(100, 100, 400, 300)), QRect(400, 170, 100, 50));
}

TEST(PopupPlacement, KeptInsideMovesBackAndCuts) {
    EXPECT_EQ(keptInside(QRect(10, 10, 100, 100), window), QRect(10, 10, 100, 100));  // already inside
    EXPECT_EQ(keptInside(QRect(750, 550, 100, 100), window), QRect(700, 500, 100, 100));
    EXPECT_EQ(keptInside(QRect(-20, 10, 900, 50), window), QRect(0, 10, 800, 50));
    EXPECT_EQ(keptInside(QRect(0, 0, 50, 50), QRect(100, 100, 400, 300)), QRect(100, 100, 50, 50));
}

TEST(PopupPlacement, TheBoundsAreTheOutermostWindow) {
    QWidget top;
    top.setGeometry(100, 80, 640, 480);
    auto* popup = new QFrame(&top, Qt::Popup);
    auto* inside = new QLabel(popup);
    EXPECT_EQ(popupBounds(inside), top.geometry());
    EXPECT_EQ(popupBounds(popup), top.geometry());
    EXPECT_EQ(popupBounds(&top), top.geometry());
    inside->setGeometry(5, 6, 30, 20);
    EXPECT_EQ(globalGeometry(inside), QRect(popup->mapToGlobal(QPoint(5, 6)), QSize(30, 20)));
}
