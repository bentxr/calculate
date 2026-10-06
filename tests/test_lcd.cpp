#include "lcd.hpp"

#include "printers.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

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

TEST(Lcd, TheKeyboardTypesTheWholeLanguage) {
    Lcd lcd;
    QTest::keyClicks(&lcd, "sqrt(2)/3+nCr(5,2)");
    EXPECT_EQ(lcd.input(), "√(2)÷3+nCr(5, 2)");
    QTest::keyClick(&lcd, Qt::Key_Home);
    QTest::keyClick(&lcd, Qt::Key_Delete);  // the root goes whole
    EXPECT_EQ(lcd.input(), "÷3+nCr(5, 2)");
    QTest::keyClick(&lcd, Qt::Key_End);
    QTest::keyClick(&lcd, Qt::Key_Backspace);
    EXPECT_EQ(lcd.input(), "÷3+nCr(5, 2");
    QTest::keyClick(&lcd, Qt::Key_Comma, Qt::KeypadModifier);  // the keypad's decimal key: always a point
    EXPECT_EQ(lcd.input(), "÷3+nCr(5, 2.");
    QTest::keyClick(&lcd, Qt::Key_Escape);
    EXPECT_EQ(lcd.input(), "");
}

TEST(Lcd, ShortcutsAreNotTyped) {
    Lcd lcd;
    QTest::keyClick(&lcd, Qt::Key_B, Qt::ControlModifier);
    QTest::keyClick(&lcd, Qt::Key_B, Qt::MetaModifier);
    EXPECT_EQ(lcd.input(), "");
}

TEST(Lcd, AnInputMethodTypesItsText) {
    Lcd lcd;
    EXPECT_TRUE(lcd.testAttribute(Qt::WA_InputMethodEnabled));
    QInputMethodEvent compose;
    compose.setCommitString(QStringLiteral("2^3"));  // a dead key's ^, or an input method's text
    QCoreApplication::sendEvent(&lcd, &compose);
    EXPECT_EQ(lcd.input(), "2^(3)");
}

TEST(Lcd, EnterEndsANameBeingTyped) {
    Lcd lcd;
    QSignalSpy evaluate(&lcd, &Lcd::evaluateRequested);
    QTest::keyClicks(&lcd, "2*pi");
    QTest::keyClick(&lcd, Qt::Key_Return);
    EXPECT_EQ(lcd.input(), "2×π");
    EXPECT_EQ(evaluate.count(), 1);
    QTest::keyClicks(&lcd, ":=");  // after a colon, = is typed (assignment), not evaluated
    EXPECT_EQ(evaluate.count(), 1);
    EXPECT_EQ(lcd.input(), "2×π:=");
}

TEST(Lcd, TheSystemKeyboardCanBeTurnedOnAndOff) {
    Lcd lcd;
    lcd.setSystemKeyboard(false);
    EXPECT_FALSE(lcd.testAttribute(Qt::WA_InputMethodEnabled));
    QTest::keyClicks(&lcd, "12");  // a physical keyboard types either way
    EXPECT_EQ(lcd.input(), "12");
    lcd.setSystemKeyboard(true);
    EXPECT_TRUE(lcd.testAttribute(Qt::WA_InputMethodEnabled));
}

TEST(Lcd, ShiftMovesAndCtrlASelect) {
    Lcd lcd;
    QTest::keyClicks(&lcd, "1+2");
    QTest::keyClick(&lcd, Qt::Key_Left, Qt::ShiftModifier);
    EXPECT_EQ(lcd.selectedText(), "2");
    QTest::keyClick(&lcd, Qt::Key_Home, Qt::ShiftModifier);
    EXPECT_EQ(lcd.selectedText(), "1+2");
    QTest::keyClicks(&lcd, "5");
    EXPECT_EQ(lcd.input(), "5");
    QTest::keyClick(&lcd, Qt::Key_A, Qt::ControlModifier);
    EXPECT_EQ(lcd.selectedText(), "5");
}

TEST(Lcd, AClickPlacesTheCursorAndADragSelects) {
    Lcd lcd;
    lcd.resize(400, 200);
    lcd.setInput("1234");
    const QPoint one = lcd.caretRectAt(Position{{}, 1}).center().toPoint();
    const QPoint three = lcd.caretRectAt(Position{{}, 3}).center().toPoint();
    QTest::mouseClick(&lcd, Qt::LeftButton, {}, one);
    EXPECT_EQ(lcd.entry().cursor(), 1);
    QTest::mousePress(&lcd, Qt::LeftButton, {}, one);
    QTest::mouseMove(&lcd, three);
    QTest::mouseRelease(&lcd, Qt::LeftButton, {}, three);
    EXPECT_EQ(lcd.selectedText(), "23");
}

TEST(Lcd, ATapPlacesTheCursor) {
    Lcd lcd;
    lcd.resize(400, 200);
    lcd.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&lcd));
    lcd.setInput("1234");
    QPointingDevice* finger = QTest::createTouchDevice();
    const QPoint one = lcd.caretRectAt(Position{{}, 1}).center().toPoint();
    QTest::touchEvent(&lcd, finger).press(0, one, &lcd);
    QTest::touchEvent(&lcd, finger).release(0, one, &lcd);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd.entry().cursor() == 1; }, 1000));
}
