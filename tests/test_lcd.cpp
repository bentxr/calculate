#include "lcd.hpp"

#include "printers.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

TEST(Lcd, TheKeyboardTypesOnlyWhatTheCalculatorHas) {
    Lcd lcd;
    QTest::keyClicks(&lcd, "abc xyz^!");
    EXPECT_EQ(lcd.input(), "");
    QTest::keyClicks(&lcd, "12+3*4/5-(6,7)");
    EXPECT_EQ(lcd.input(), "12+3×4÷5−(6.7)");
    QTest::keyClick(&lcd, Qt::Key_Backspace);
    EXPECT_EQ(lcd.input(), "12+3×4÷5−(6.7");
    QTest::keyClick(&lcd, Qt::Key_Left);
    QTest::keyClicks(&lcd, "0");
    EXPECT_EQ(lcd.input(), "12+3×4÷5−(6.07");
    QTest::keyClick(&lcd, Qt::Key_Escape);
    EXPECT_EQ(lcd.input(), "");
}

TEST(Lcd, PasteDoesNothing) {
    Lcd lcd;
    QGuiApplication::clipboard()->setText("sin(1)");
    QTest::keySequence(&lcd, QKeySequence::Paste);
    EXPECT_EQ(lcd.input(), "");
}

TEST(Lcd, EnterEvaluatesAndUpDownAskForTheHistory) {
    Lcd lcd;
    QSignalSpy evaluate(&lcd, &Lcd::evaluateRequested);
    QSignalSpy history(&lcd, &Lcd::historyRequested);
    QTest::keyClick(&lcd, Qt::Key_Return);
    QTest::keyClick(&lcd, Qt::Key_Enter);
    QTest::keyClicks(&lcd, "=");
    EXPECT_EQ(evaluate.count(), 3);
    QTest::keyClick(&lcd, Qt::Key_Up);
    QTest::keyClick(&lcd, Qt::Key_Down);
    ASSERT_EQ(history.count(), 2);
    EXPECT_EQ(history.at(0).at(0).toInt(), 1);
    EXPECT_EQ(history.at(1).at(0).toInt(), -1);
}

TEST(Lcd, TheOutputIsTheValueOnly) {
    Lcd lcd;
    lcd.showValue({"0.300000000000000", "0444089209850062616169452667236328125", ""});
    EXPECT_EQ(lcd.outputText(), "0.300000000000000|0444089209850062616169452667236328125");
    lcd.showValue({"−1.000000000000000", "019884624838656", "30"});
    EXPECT_EQ(lcd.outputText(), "−1.000000000000000|019884624838656×10^30");
    lcd.showExact({"", "1", "3", "0.", "3"});
    EXPECT_EQ(lcd.outputText(), "1/3 = 0.(3)");
    lcd.showExact({"−", "6", "1", "", ""});
    EXPECT_EQ(lcd.outputText(), "−6");
    lcd.clearResult();
    EXPECT_EQ(lcd.outputText(), "");
    lcd.showValue({"4", "", ""});
    lcd.clear();
    EXPECT_EQ(lcd.outputText(), "");
}

TEST(Lcd, TheScreenUsesTheBundledFont) {
    EXPECT_EQ(Lcd::fontFamily(), "JetBrains Mono");
}

TEST(Lcd, ItsColoursFollowTheTheme) {
    Lcd lcd;
    QPalette light;
    light.setColor(QPalette::Window, Qt::white);
    lcd.setPalette(light);
    EXPECT_GT(lcd.background().lightness(), 150);  // a pale panel with dark ink
    EXPECT_LT(lcd.ink().lightness(), 80);
    EXPECT_GT(lcd.noiseColor().lightness(), lcd.ink().lightness());
    EXPECT_LT(lcd.noiseColor().lightness(), lcd.background().lightness());
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30, 30, 30));
    lcd.setPalette(dark);
    EXPECT_LT(lcd.background().lightness(), 80);
    EXPECT_GT(lcd.ink().lightness(), 150);
    EXPECT_LT(lcd.noiseColor().lightness(), lcd.ink().lightness());
    EXPECT_GT(lcd.noiseColor().lightness(), lcd.background().lightness());
}

TEST(Lcd, TheStatusLineShowsTheMemory) {
    Lcd lcd;
    EXPECT_EQ(lcd.statusText(), "");
    lcd.setMemory("5");
    EXPECT_EQ(lcd.statusText(), "M");
    EXPECT_EQ(lcd.toolTip(), "M = 5");
    lcd.setMemory({});
    EXPECT_EQ(lcd.statusText(), "");
    EXPECT_EQ(lcd.toolTip(), "");
}

TEST(Lcd, InsideAFractionUpAndDownStayInTheEntry) {
    Lcd lcd;
    QSignalSpy history(&lcd, &Lcd::historyRequested);
    lcd.insertTemplate(Template::Fraction);
    QTest::keyClicks(&lcd, "1");
    QTest::keyClick(&lcd, Qt::Key_Down);
    QTest::keyClicks(&lcd, "2");
    EXPECT_EQ(lcd.input(), "((1)/(2))");
    EXPECT_EQ(history.count(), 0);
    QTest::keyClick(&lcd, Qt::Key_Right);  // out of the fraction
    QTest::keyClick(&lcd, Qt::Key_Up);
    EXPECT_EQ(history.count(), 1);
}
