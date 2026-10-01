#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "lcd.hpp"
#include "presenter.hpp"
#include "typechooser.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>

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

// The value in a row of the Details card.
QString detail(MainWindow& window, const QString& key) {
    auto* label = child<DetailsCard>(window, "detailsCard")->findChild<QLabel*>("value:" + key);
    return label ? label->text() : QString();
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
    auto* type = child<TypeChooser>(window, "type");
    ASSERT_EQ(type->count(), 7);
    EXPECT_EQ(type->currentIndex(), 1);
    EXPECT_EQ(type->itemText(1), "Double");
    EXPECT_EQ(type->itemText(6), "Hexadecuple");
    EXPECT_EQ(type->itemData(1, TypeChooser::DigitsRole).toString(), "~16 significant digits");
    EXPECT_EQ(type->itemData(1, TypeChooser::DetailRole).toString(), "double · 64-bit storage · 53-bit significand");
    EXPECT_EQ(type->itemData(1, Qt::ToolTipRole).toString(), "Double · double · 64-bit · ~16 digits");
    EXPECT_GT(type->view()->minimumWidth(), 2 * type->sizeHint().width());  // the menu is wider than the button
    EXPECT_EQ(child<QComboBox>(window, "angle")->count(), 3);
}

TEST(MainWindow, AngleTypeAndEqualsStackBesideTheScreen) {
    MainWindow window;
    window.resize(900, 700);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const QRect angle = child<QWidget>(window, "angle")->geometry();
    const QRect type = child<QWidget>(window, "type")->geometry();
    const QRect equals = child<QWidget>(window, "equals")->geometry();
    const QRect screen = child<QWidget>(window, "lcd")->geometry();
    EXPECT_EQ(angle.width(), type.width());
    EXPECT_EQ(type.width(), equals.width());
    EXPECT_EQ(angle.x(), type.x());
    EXPECT_EQ(type.x(), equals.x());
    EXPECT_LT(angle.bottom(), type.top());
    EXPECT_LT(type.bottom(), equals.top());
    EXPECT_EQ(screen.top(), angle.top());
    EXPECT_EQ(screen.bottom(), equals.bottom());
    EXPECT_LT(screen.right(), angle.left());
    EXPECT_LT(type.width(), screen.width() / 3);
}

TEST(MainWindow, EvaluatesAndShowsTheErrorReport) {
    MainWindow window;
    run(window, "0.1 + 0.2");
    EXPECT_EQ(lcd(window)->outputText(), "0.300000000000000|0444089209850062616169452667236328125");
    EXPECT_EQ(detail(window, "bound"), "4.4e-17");
    EXPECT_EQ(detail(window, "trusted"), "15 by the bound, 15 by the measurement");
    EXPECT_EQ(detail(window, "type"), "double, 53-bit significand");
    QListWidgetItem* item = child<QListWidget>(window, "history")->item(0);
    EXPECT_EQ(item->data(Qt::UserRole).toString(), "0.1 + 0.2");
    EXPECT_EQ(item->text(), "0.1 + 0.2 = 0.300000000000000|0444089209…");
}

TEST(MainWindow, ChangingTheTypeReevaluates) {
    MainWindow window;
    run(window, "1/3");
    lcd(window)->showMessage({});
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1/3 = 0.(3)");
    EXPECT_EQ(detail(window, "exact"), "exact · no rounding error");
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
    EXPECT_EQ(detail(window, "incomplete"), "an uncertain argument was accepted");
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

TEST(MainWindow, TheScreenShowsOnlyTheValueAndTheCardTheRest) {
    MainWindow window;
    window.resize(900, 700);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    EXPECT_EQ(window.findChild<QLabel*>("errorLine"), nullptr);
    EXPECT_EQ(window.findChild<QLabel*>("whyLine"), nullptr);
    auto* button = child<QToolButton>(window, "detailsButton");
    EXPECT_FALSE(button->isEnabled());  // nothing to explain yet
    run(window, "0.1 + 0.2");
    EXPECT_TRUE(button->isEnabled());
    auto* card = child<DetailsCard>(window, "detailsCard");
    EXPECT_FALSE(card->isVisible());
    const QRect keypad = child<QWidget>(window, "keypad")->geometry();
    const QRect screen = lcd(window)->geometry();
    QTest::mouseClick(button, Qt::LeftButton);
    EXPECT_TRUE(card->isVisible());
    EXPECT_EQ(child<QWidget>(window, "keypad")->geometry(), keypad);  // an overlay: nothing moves
    EXPECT_EQ(lcd(window)->geometry(), screen);
    card->hide();
}

TEST(MainWindow, EveryRowOfTheCardExplainsItself) {
    MainWindow window;
    run(window, "0.1 + 0.2");
    auto* card = child<DetailsCard>(window, "detailsCard");
    for (const char* key : {"bound", "measured", "trusted", "condition", "input", "rounding", "library", "operations",
                            "type", "evaluated"}) {
        auto* info = card->findChild<QToolButton*>(QString("info:") + key);
        auto* text = card->findChild<QLabel*>(QString("explanation:") + key);
        ASSERT_NE(info, nullptr) << key;
        ASSERT_NE(text, nullptr) << key;
        EXPECT_FALSE(text->isVisibleTo(card));
        QTest::mouseClick(info, Qt::LeftButton);
        EXPECT_TRUE(text->isVisibleTo(card)) << key;
        EXPECT_EQ(text->text(), view::explanation(key));
    }
}

TEST(MainWindow, TheHistoryDropsDownUnderTheScreen) {
    MainWindow window;
    window.resize(900, 700);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* toggle = child<QToolButton>(window, "historyToggle");
    EXPECT_FALSE(toggle->isEnabled());  // nothing to show yet
    run(window, "1+1");
    run(window, "2+2");
    EXPECT_TRUE(toggle->isEnabled());
    auto* panel = child<QWidget>(window, "historyPanel");
    auto* list = child<QListWidget>(window, "history");
    const QRect keypad = child<QWidget>(window, "keypad")->geometry();
    QTest::mouseClick(toggle, Qt::LeftButton);
    EXPECT_TRUE(panel->isVisible());
    EXPECT_EQ(child<QWidget>(window, "keypad")->geometry(), keypad);  // an overlay: nothing moves
    ASSERT_EQ(list->count(), 2);
    EXPECT_EQ(list->item(0)->text(), "2+2 = 4");
    EXPECT_EQ(list->item(1)->text(), "1+1 = 2");
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(1)).center());
    EXPECT_EQ(lcd(window)->input(), "1+1");
    EXPECT_FALSE(panel->isVisible());
}
