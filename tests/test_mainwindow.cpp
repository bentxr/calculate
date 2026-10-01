#include "mainwindow.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPalette>
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

// Types an expression, presses Enter and waits for the worker's answer.
void run(MainWindow& window, const QString& expression) {
    auto* input = child<QLineEdit>(window, "expression");
    auto* value = child<QLabel>(window, "value");
    auto* message = child<QLabel>(window, "message");
    value->clear();
    message->clear();
    input->setText(expression);
    QTest::keyClick(input, Qt::Key_Return);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !value->text().isEmpty() || !message->text().isEmpty(); }, 10000))
        << expression.toStdString();
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
    EXPECT_TRUE(child<QLabel>(window, "value")->text().startsWith("0.300000000000000<span"));
    EXPECT_EQ(child<QLabel>(window, "errorLine")->text(), "± 4.4e-17 · 15 trusted digits");
    EXPECT_FALSE(child<QLabel>(window, "whyLine")->text().isEmpty());
    EXPECT_EQ(child<QListWidget>(window, "history")->item(0)->text(), "0.1 + 0.2");
}

TEST(MainWindow, ChangingTheTypeReevaluates) {
    MainWindow window;
    run(window, "1/3");
    auto* value = child<QLabel>(window, "value");
    value->clear();
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    EXPECT_TRUE(QTest::qWaitFor([&] { return !value->text().isEmpty(); }, 10000));
    EXPECT_EQ(value->text(), "1/3 = 0.(3)");
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
    auto* input = child<QLineEdit>(window, "expression");
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:exponent"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:close"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "sin(π)");
    QTest::mouseClick(child<QPushButton>(window, "key:delete"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "sin(π");
    QTest::mouseClick(child<QPushButton>(window, "key:clear"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "");
}

TEST(MainWindow, ShiftAndAlphaLastForOneKey) {
    MainWindow window;
    auto* input = child<QLineEdit>(window, "expression");
    auto* shift = child<QPushButton>(window, "key:shift");
    QTest::mouseClick(shift, Qt::LeftButton);
    EXPECT_TRUE(shift->isChecked());
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    EXPECT_FALSE(shift->isChecked());
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "asin(sin(");
    input->clear();
    QTest::mouseClick(child<QPushButton>(window, "key:alpha"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:multiply"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:multiply"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "gcd(×");
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
    auto* value = child<QLabel>(window, "value");
    QTest::mouseClick(proceed, Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !value->text().isEmpty(); }, 10000));
    EXPECT_EQ(value->text(), "6");
    EXPECT_TRUE(child<QLabel>(window, "errorLine")->text().contains("incomplete"));
    EXPECT_FALSE(proceed->isVisibleTo(&window));
}

TEST(MainWindow, MemoryKeys) {
    MainWindow window;
    run(window, "5");
    auto* memory = child<QLabel>(window, "memory");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !memory->text().isEmpty(); }, 10000));
    EXPECT_EQ(memory->text(), "M = 5");
    QTest::mouseClick(child<QPushButton>(window, "key:shift"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return memory->text() == "M = 5-(5)"; }, 10000)) << memory->text().toStdString();
}

TEST(MainWindow, StatisticsModeBuildsAnExpression) {
    MainWindow window;
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    EXPECT_EQ(child<QStackedWidget>(window, "pages")->currentIndex(), 1);
    child<QPlainTextEdit>(window, "statisticsValues")->setPlainText("2\n4\n4\n4\n5\n5\n7\n9");
    auto* value = child<QLabel>(window, "value");
    QTest::mouseClick(child<QPushButton>(window, "stat:stdevp"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !value->text().isEmpty(); }, 10000));
    EXPECT_EQ(child<QLineEdit>(window, "expression")->text(), "stdevp(2, 4, 4, 4, 5, 5, 7, 9)");
    EXPECT_EQ(value->text(), "2");
}

TEST(MainWindow, TheNoiseColourStaysVisibleWhenThePaletteIsTranslucent) {
    MainWindow window;
    QPalette palette = window.palette();
    palette.setColor(QPalette::Window, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(0, 0, 0, 128));  // as in Fusion
    window.setPalette(palette);
    run(window, "0.1 + 0.2");
    EXPECT_TRUE(child<QLabel>(window, "value")->text().contains("color:#7f7f7f")) << child<QLabel>(window, "value")->text().toStdString();
}

TEST(MainWindow, UpAndDownReplayTheHistory) {
    MainWindow window;
    run(window, "1+1");
    run(window, "2+2");
    auto* input = child<QLineEdit>(window, "expression");
    auto* up = child<QPushButton>(window, "key:up");
    input->clear();
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(input->text(), "2+2");
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(input->text(), "1+1");
    QTest::mouseClick(up, Qt::LeftButton);
    EXPECT_EQ(input->text(), "1+1");
    QTest::mouseClick(child<QPushButton>(window, "key:down"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "2+2");
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
    auto* input = child<QLineEdit>(window, "expression");
    QTest::mouseClick(child<QPushButton>(window, "key:options"), Qt::LeftButton);
    EXPECT_TRUE(options->isVisible());
    action(options, "sinh")->trigger();
    options->hide();
    EXPECT_EQ(input->text(), "sinh(");
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    QTest::mouseClick(child<QPushButton>(window, "key:options"), Qt::LeftButton);
    EXPECT_FALSE(action(options, "sinh")->isEnabled());
    EXPECT_TRUE(action(options, "mod")->isEnabled());
    options->hide();
    run(window, "5");
    auto* memory = child<QLabel>(window, "memory");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !memory->text().isEmpty(); }, 10000));
    action(options, "MC")->trigger();
    EXPECT_TRUE(QTest::qWaitFor([&] { return memory->text().isEmpty(); }, 10000));
}
