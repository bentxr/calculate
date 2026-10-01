#include "mainwindow.hpp"

#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QTest>

#include <gtest/gtest.h>

// Renders the window once per digit style, for choosing the display (skipped unless
// CALCULATE_SCREENSHOTS names an output directory).
TEST(Screenshots, DigitStyles) {
    const QString directory = qEnvironmentVariable("CALCULATE_SCREENSHOTS");
    if (directory.isEmpty()) GTEST_SKIP() << "set CALCULATE_SCREENSHOTS to a directory";
    QDir().mkpath(directory);
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    auto* input = window.findChild<QLineEdit*>("expression");
    auto* value = window.findChild<QLabel*>("value");
    input->setText("sin(1e10)");
    QTest::keyClick(input, Qt::Key_Return);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !value->text().isEmpty(); }, 10000));
    const std::pair<view::DigitStyle, const char*> styles[] = {
        {view::DigitStyle::Faded, "faded"}, {view::DigitStyle::Bold, "bold"}, {view::DigitStyle::Bar, "bar"}};
    for (const auto& [style, name] : styles) {
        window.setDigitStyle(style);
        QTest::qWait(50);
        EXPECT_TRUE(window.grab().save(directory + "/" + name + ".png"));
    }
}
