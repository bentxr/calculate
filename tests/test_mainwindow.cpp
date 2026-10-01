#include "mainwindow.hpp"

#include "lcd.hpp"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTest>

#include "printers.hpp"

#include <gtest/gtest.h>

namespace {

template <class W>
W* child(MainWindow& window, const char* name) {
    W* w = window.findChild<W*>(name);
    EXPECT_NE(w, nullptr) << name;
    return w;
}

Lcd* lcd(MainWindow& window) { return child<Lcd>(window, "lcd"); }

// Waits for the worker's answer on the screen.
bool answered(MainWindow& window) {
    return QTest::qWaitFor([&] { return !lcd(window)->outputText().isEmpty(); }, 10000);
}

// Enters an expression, presses Enter and waits for the answer.
void run(MainWindow& window, const QString& expression) {
    lcd(window)->clear();
    lcd(window)->setInput(expression);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window)) << expression.toStdString();
}

QAction* action(QMenu* menu, const QString& text) {
    for (QAction* a : menu->actions())
        if (a->text() == text) return a;
    ADD_FAILURE() << "no action " << text.toStdString();
    return nullptr;
}

}  // namespace

TEST(MainWindow, TheTypeMenuComesFromTheEngine) {
    MainWindow window;
    auto* type = child<QComboBox>(window, "type");
    ASSERT_EQ(type->count(), 7);
    EXPECT_EQ(type->currentIndex(), 1);
    EXPECT_EQ(type->itemText(1), "Double · double · 64-bit · ~16 digits");
    EXPECT_EQ(child<QComboBox>(window, "angle")->count(), 3);
}

TEST(MainWindow, EvaluatesAndShowsTheErrorReport) {
    MainWindow window;
    run(window, "0.1 + 0.2");
    EXPECT_EQ(lcd(window)->outputText(), "0.300000000000000|0444089209850062616169452667236328125");
    EXPECT_EQ(child<QLabel>(window, "errorLine")->text(), "± 4.4e-17 · 15 trusted digits");
    EXPECT_FALSE(child<QLabel>(window, "whyLine")->text().isEmpty());
    EXPECT_EQ(child<QListWidget>(window, "history")->item(0)->text(), "0.1 + 0.2");
}

TEST(MainWindow, ChangingTheTypeReevaluates) {
    MainWindow window;
    run(window, "1/3");
    lcd(window)->showMessage({});
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1/3 = 0.(3)");
    EXPECT_EQ(child<QLabel>(window, "errorLine")->text(), "exact · no rounding error");
}

TEST(MainWindow, ExactModeGreysOutTranscendentalKeys) {
    MainWindow window;
    auto* sin = child<QPushButton>(window, "key:sin");
    EXPECT_TRUE(sin->isEnabled());
    child<QComboBox>(window, "type")->setCurrentIndex(3);
    EXPECT_FALSE(sin->isEnabled());
    EXPECT_FALSE(sin->toolTip().isEmpty());
    EXPECT_TRUE(child<QPushButton>(window, "key:sqrt")->isEnabled());
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    EXPECT_FALSE(child<QPushButton>(window, "key:exponent")->isEnabled());  // SHIFT: π
    EXPECT_TRUE(child<QPushButton>(window, "key:reciprocal")->isEnabled());  // SHIFT: x!
}

TEST(MainWindow, TheKeypadEditsTheExpression) {
    MainWindow window;
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:exponent"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:close"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(π)");
    QTest::mouseClick(child<QPushButton>(window, "key:delete"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(π");
    QTest::mouseClick(child<QPushButton>(window, "key:clear"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "");
}

TEST(MainWindow, ShiftAndAlphaLastForOneKey) {
    MainWindow window;
    auto* shift = child<QPushButton>(window, "key:shift");
    QTest::mouseClick(shift, Qt::LeftButton);
    EXPECT_TRUE(shift->isChecked());
    EXPECT_EQ(lcd(window)->statusText(), "S");
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    EXPECT_FALSE(shift->isChecked());
    EXPECT_EQ(lcd(window)->statusText(), "");
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "asin(sin(");
    lcd(window)->clear();
    QTest::mouseClick(child<QPushButton>(window, "key:alpha"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:multiply"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:multiply"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "gcd(×");
}

TEST(MainWindow, KeysThisAppCannotUseYetStayInPlaceDisabled) {
    MainWindow window;
    auto* calc = child<QPushButton>(window, "key:calc");
    EXPECT_FALSE(calc->isEnabled());
    EXPECT_FALSE(calc->toolTip().isEmpty());
    QTest::mouseClick(child<QPushButton>(window, "key:alpha"), Qt::LeftButton);
    EXPECT_FALSE(child<QPushButton>(window, "key:7")->isEnabled());  // no ALPHA function
    EXPECT_TRUE(child<QPushButton>(window, "key:memoryAdd")->isEnabled());  // ALPHA: M
}

TEST(MainWindow, UncertainArgumentsOfferToProceed) {
    MainWindow window;
    auto* proceed = child<QPushButton>(window, "proceed");
    run(window, "(0.1*30)!");
    EXPECT_TRUE(proceed->isVisibleTo(&window));
    lcd(window)->showMessage({});
    QTest::mouseClick(proceed, Qt::LeftButton);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "6");
    EXPECT_TRUE(child<QLabel>(window, "errorLine")->text().contains("incomplete"));
    EXPECT_FALSE(proceed->isVisibleTo(&window));
}

TEST(MainWindow, MemoryKeys) {
    MainWindow window;
    run(window, "5");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory() == "5"; }, 10000));
    EXPECT_EQ(lcd(window)->statusText(), "M");
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory() == "5-(5)"; }, 10000)) << lcd(window)->memory().toStdString();
}

TEST(MainWindow, StatisticsModeBuildsAnExpression) {
    MainWindow window;
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    EXPECT_EQ(child<QStackedWidget>(window, "pages")->currentIndex(), 1);
    child<QPlainTextEdit>(window, "statisticsValues")->setPlainText("2\n4\n4\n4\n5\n5\n7\n9");
    QTest::mouseClick(child<QPushButton>(window, "stat:stdevp"), Qt::LeftButton);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->input(), "stdevp(2, 4, 4, 4, 5, 5, 7, 9)");
    EXPECT_EQ(lcd(window)->outputText(), "2");
}

TEST(MainWindow, UpAndDownReplayTheHistory) {
    MainWindow window;
    run(window, "1+1");
    run(window, "2+2");
    auto* up = child<QPushButton>(window, "key:up");
    lcd(window)->clear();
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "2+2");
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "1+1");
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "1+1");
    QTest::mouseClick(child<QPushButton>(window, "key:down"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "2+2");
}

TEST(MainWindow, MenuChoosesTheModeAndConfigTheAngle) {
    MainWindow window;
    auto* menu = child<QMenu>(window, "menu");
    QTest::mouseClick(child<QPushButton>(window, "key:menu"), Qt::LeftButton);
    EXPECT_TRUE(menu->isVisible());
    action(menu, "Statistics")->trigger();
    menu->hide();
    EXPECT_EQ(child<QStackedWidget>(window, "pages")->currentIndex(), 1);
    auto* config = child<QMenu>(window, "config");
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:menu"), Qt::LeftButton);
    EXPECT_TRUE(config->isVisible());
    action(config, "DEG")->trigger();
    config->hide();
    EXPECT_EQ(child<QComboBox>(window, "angle")->currentIndex(), 1);
}

TEST(MainWindow, OptionsHoldTheHyperbolicFunctionsModAndMemoryClear) {
    MainWindow window;
    auto* options = child<QMenu>(window, "options");
    QTest::mouseClick(child<QPushButton>(window, "key:options"), Qt::LeftButton);
    EXPECT_TRUE(options->isVisible());
    action(options, "sinh")->trigger();
    options->hide();
    EXPECT_EQ(lcd(window)->input(), "sinh(");
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    QTest::mouseClick(child<QPushButton>(window, "key:options"), Qt::LeftButton);
    EXPECT_FALSE(action(options, "sinh")->isEnabled());
    EXPECT_TRUE(action(options, "mod")->isEnabled());
    options->hide();
    run(window, "5");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !lcd(window)->memory().isEmpty(); }, 10000));
    action(options, "MC")->trigger();
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory().isEmpty(); }, 10000));
}

TEST(MainWindow, CancelStaysOfferedWhileALaterRequestRuns) {
    MainWindow window;
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    lcd(window)->setInput("1 + 1");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    lcd(window)->setInput("200000!");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    auto* cancel = child<QPushButton>(window, "cancel");
    ASSERT_TRUE(QTest::qWaitFor([&] { return cancel->isVisibleTo(&window); }, 5000));
    QTest::mouseClick(cancel, Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "Cancelled"; }, 10000))
        << lcd(window)->outputText().toStdString();
    EXPECT_FALSE(cancel->isVisibleTo(&window));
}

TEST(MainWindow, KeysLeaveTheKeyboardToTheScreen) {
    MainWindow window;
    EXPECT_EQ(child<QPushButton>(window, "key:7")->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(child<QPushButton>(window, "key:shift")->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(lcd(window)->focusPolicy(), Qt::StrongFocus);
    QTest::mouseClick(child<QPushButton>(window, "key:7"), Qt::LeftButton);
    QTest::keyClicks(lcd(window), "*6");
    EXPECT_EQ(lcd(window)->input(), "7×6");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "42");
}
