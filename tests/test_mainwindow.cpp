#include "mainwindow.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
    EXPECT_TRUE(child<QPushButton>(window, "key:√")->isEnabled());
}

TEST(MainWindow, TheKeypadEditsTheExpression) {
    MainWindow window;
    auto* input = child<QLineEdit>(window, "expression");
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:π"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:)"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "sin(π)");
    QTest::mouseClick(child<QPushButton>(window, "key:⌫"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "sin(π");
    QTest::mouseClick(child<QPushButton>(window, "key:C"), Qt::LeftButton);
    EXPECT_EQ(input->text(), "");
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
    QTest::mouseClick(child<QPushButton>(window, "key:M+"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !memory->text().isEmpty(); }, 10000));
    EXPECT_EQ(memory->text(), "M = 5");
    QTest::mouseClick(child<QPushButton>(window, "key:MC"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return memory->text().isEmpty(); }, 10000));
}
