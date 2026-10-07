#include "keysizing.hpp"

#include <QFontMetrics>

#include <gtest/gtest.h>

// A 1920 × 1200 screen; the window needs 200 px across and 400 px down for everything but the keys;
// a grid of 6 × 9 keys with 6 px between them.
TEST(KeySizing, HalfTheScreenSharedByTheColumns) {
    // across: (1920 / 2 − 200 − 5 × 6) / 6 = 121; down: (1200 − 400 − 8 × 6) / 9 = 83 → 83 / 0.55 = 151.
    EXPECT_EQ(keySize(QSize(1920, 1200), QSize(200, 400), QSize(6, 9), 6, QSize(40, 24)), QSize(121, 67));
    EXPECT_EQ(keySize(QSize(1280, 1200), QSize(200, 400), QSize(6, 9), 6, QSize(40, 24)), QSize(68, 37));
    EXPECT_EQ(keySize(QSize(1920, 1200), QSize(200, 400), QSize(12, 9), 6, QSize(40, 24)), QSize(57, 31));
}

TEST(KeySizing, AShortScreenLimitsTheHeight) {
    // down: (800 − 400 − 48) / 9 = 39 → 39 / 0.55 = 71, narrower than the 121 that fits across.
    EXPECT_EQ(keySize(QSize(1920, 800), QSize(200, 400), QSize(6, 9), 6, QSize(40, 24)), QSize(71, 39));
}

TEST(KeySizing, NeverSmallerThanTheLabelsNeed) {
    EXPECT_EQ(keySize(QSize(800, 600), QSize(200, 400), QSize(6, 9), 6, QSize(40, 24)), QSize(40, 24));
    EXPECT_EQ(keySize(QSize(0, 0), QSize(200, 400), QSize(6, 9), 6, QSize(40, 24)), QSize(40, 24));
}

TEST(KeySizing, ALabelThatFitsKeepsItsFont) {
    QFont font;
    font.setPixelSize(20);
    EXPECT_EQ(fittedFont(font, "sin", 200), font);
}

TEST(KeySizing, ALongLabelGetsASmallerFontThatFits) {
    QFont font;
    font.setPixelSize(20);
    const int width = QFontMetrics(font).horizontalAdvance("floatError") * 3 / 4;
    const QFont fitted = fittedFont(font, "floatError", width);
    EXPECT_LT(fitted.pixelSize(), 20);
    EXPECT_GE(fitted.pixelSize(), 12);
    EXPECT_LE(QFontMetrics(fitted).horizontalAdvance("floatError"), width);
}

TEST(KeySizing, NeverSmallerThanTheSmallestSize) {
    QFont font;
    font.setPixelSize(20);
    EXPECT_EQ(fittedFont(font, "floatError", 5).pixelSize(), 12);  // 0.6 × 20
}

TEST(KeySizing, PortraitScreensGetThePhoneArrangement) {
    EXPECT_EQ(keysLayout(QSize(390, 844)), KeysLayout::Narrow);   // a phone
    EXPECT_EQ(keysLayout(QSize(768, 1024)), KeysLayout::Narrow);  // a tablet held upright
    EXPECT_EQ(keysLayout(QSize(1920, 1200)), KeysLayout::Wide);
    EXPECT_EQ(keysLayout(QSize(800, 600)), KeysLayout::Wide);
    EXPECT_EQ(keysLayout(QSize(844, 390)), KeysLayout::Wide);     // a phone on its side
    EXPECT_EQ(keysLayout(QSize(1000, 1000)), KeysLayout::Wide);
}

TEST(KeySizing, PhoneKeysShareTheWidthAndStayTouchSized) {
    EXPECT_EQ(touchTarget, 44);
    // (390 − 16 − 5 × 6) / 6 = 57; 0.55 × 57 = 31, under the touch target: 44
    EXPECT_EQ(phoneKeySize(QSize(390, 844), 16, 6, 6, QSize(36, 44)), QSize(57, 44));
    // never wider than twice the minimum: (1200 − 16 − 30) / 6 = 192 → 72; 0.55 × 72 = 40 → 44
    EXPECT_EQ(phoneKeySize(QSize(1200, 1920), 16, 6, 6, QSize(36, 44)), QSize(72, 44));
    // never narrower than the minimum
    EXPECT_EQ(phoneKeySize(QSize(200, 844), 16, 6, 6, QSize(36, 44)), QSize(36, 44));
    // a taller minimum wins over the touch target
    EXPECT_EQ(phoneKeySize(QSize(390, 844), 16, 6, 6, QSize(36, 50)), QSize(57, 50));
}
