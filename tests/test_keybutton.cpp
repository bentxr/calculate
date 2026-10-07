#include "keybutton.hpp"

#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

TEST(KeyButton, AClickIsAClick) {
    KeyButton key;
    key.setHasMore(true);
    key.resize(60, 30);
    QSignalSpy clicked(&key, &QPushButton::clicked);
    QSignalSpy more(&key, &KeyButton::moreRequested);
    QTest::mouseClick(&key, Qt::LeftButton);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(more.count(), 0);
}

TEST(KeyButton, ALongPressOffersTheOtherFacesAndDoesNotClick) {
    KeyButton key;
    key.setHasMore(true);
    key.resize(60, 30);
    QSignalSpy clicked(&key, &QPushButton::clicked);
    QSignalSpy more(&key, &KeyButton::moreRequested);
    QTest::mousePress(&key, Qt::LeftButton);
    QTest::qWait(KeyButton::longPressMs + 200);
    EXPECT_EQ(more.count(), 1);
    QTest::mouseRelease(&key, Qt::LeftButton);
    EXPECT_EQ(clicked.count(), 0);
}

TEST(KeyButton, ARightClickOffersThem) {
    KeyButton key;
    key.setHasMore(true);
    key.resize(60, 30);
    QSignalSpy clicked(&key, &QPushButton::clicked);
    QSignalSpy more(&key, &KeyButton::moreRequested);
    QTest::mouseClick(&key, Qt::RightButton);
    EXPECT_EQ(more.count(), 1);
    EXPECT_EQ(clicked.count(), 0);
}

TEST(KeyButton, AKeyWithNothingMoreJustClicks) {
    KeyButton key;
    key.resize(60, 30);
    QSignalSpy clicked(&key, &QPushButton::clicked);
    QSignalSpy more(&key, &KeyButton::moreRequested);
    QTest::mousePress(&key, Qt::LeftButton);
    QTest::qWait(KeyButton::longPressMs + 200);
    QTest::mouseRelease(&key, Qt::LeftButton);
    EXPECT_EQ(more.count(), 0);
    EXPECT_EQ(clicked.count(), 1);
}
