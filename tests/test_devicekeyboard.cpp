#include "devicekeyboard.hpp"

#include "lcd.hpp"
#include "printers.hpp"

#include <QCoreApplication>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTest>

#include <gtest/gtest.h>

namespace {

// What the platform asks a box before it brings the device's keyboard up.
bool takesAnInputMethod(QWidget* widget) {
    QInputMethodQueryEvent query(Qt::ImEnabled);
    QCoreApplication::sendEvent(widget, &query);
    return query.value(Qt::ImEnabled).toBool();
}

}  // namespace

TEST(DeviceKeyboard, StartsAllowedWhereTheKeyboardIsPhysical) {
    DeviceKeyboard keyboard;
    EXPECT_TRUE(DeviceKeyboard::startAllowed);  // the desktop: its keyboard's dead keys are an input method
    EXPECT_EQ(keyboard.allowed(), DeviceKeyboard::startAllowed);
}

TEST(DeviceKeyboard, WhileOffNoBoxTakesAnInputMethod) {
    DeviceKeyboard keyboard;
    Lcd lcd;
    QLineEdit line;
    QPlainTextEdit text;
    const QList<QWidget*> boxes{&lcd, &line, &text};
    for (QWidget* box : boxes) EXPECT_TRUE(takesAnInputMethod(box)) << box->metaObject()->className();
    keyboard.setAllowed(false);
    EXPECT_FALSE(keyboard.allowed());
    for (QWidget* box : boxes) EXPECT_FALSE(takesAnInputMethod(box)) << box->metaObject()->className();
    QLineEdit later;  // a box made while off
    EXPECT_FALSE(takesAnInputMethod(&later));
    keyboard.setAllowed(true);
    EXPECT_TRUE(keyboard.allowed());
    for (QWidget* box : boxes) EXPECT_TRUE(takesAnInputMethod(box)) << box->metaObject()->className();
    EXPECT_TRUE(takesAnInputMethod(&later));
}

TEST(DeviceKeyboard, APhysicalKeyboardTypesEitherWay) {
    DeviceKeyboard keyboard;
    Lcd lcd;
    keyboard.setAllowed(false);
    QTest::keyClicks(&lcd, "12");
    EXPECT_EQ(lcd.input(), "12");
    keyboard.setAllowed(true);
    QInputMethodEvent compose;
    compose.setCommitString(QStringLiteral("3"));  // the device keyboard's text
    QCoreApplication::sendEvent(&lcd, &compose);
    EXPECT_EQ(lcd.input(), "123");
}

TEST(DeviceKeyboard, StopsDecidingWhenItIsGone) {
    QLineEdit line;
    {
        DeviceKeyboard keyboard;
        keyboard.setAllowed(false);
        EXPECT_FALSE(takesAnInputMethod(&line));
    }
    EXPECT_TRUE(takesAnInputMethod(&line));
}
