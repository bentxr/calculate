#include "detailscard.hpp"
#include "inspector.hpp"
#include "lcd.hpp"
#include "mainwindow.hpp"
#include "settings.hpp"

#include <QDir>
#include <QLabel>
#include <QTest>
#include <QToolButton>

#include <gtest/gtest.h>

// Pictures for review (skipped unless CALCULATE_SCREENSHOTS names an output directory).
TEST(Screenshots, StoredBits) {
    const QString directory = qEnvironmentVariable("CALCULATE_SCREENSHOTS");
    if (directory.isEmpty()) GTEST_SKIP() << "set CALCULATE_SCREENSHOTS to a directory";
    QDir().mkpath(directory);
    for (const bool dark : {false, true}) {
        settings::setTheme(dark ? settings::Theme::Dark : settings::Theme::Light);
        MainWindow window;
        window.resize(1000, 900);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        auto* lcd = window.findChild<Lcd*>("lcd");
        lcd->setInput("0.1 + 0.2");
        QTest::keyClick(lcd, Qt::Key_Return);
        ASSERT_TRUE(QTest::qWaitFor([&] { return !lcd->outputText().isEmpty(); }, 10000));
        QTest::mouseClick(window.findChild<QToolButton*>("detailsButton"), Qt::LeftButton);
        auto* card = window.findChild<DetailsCard*>("detailsCard");
        QTest::qWait(100);
        EXPECT_TRUE(card->grab().save(directory + (dark ? "/details-dark.png" : "/details-light.png")));
        card->hide();
    }
    settings::setTheme(settings::Theme::System);
}

TEST(Screenshots, Inspector) {
    const QString directory = qEnvironmentVariable("CALCULATE_SCREENSHOTS");
    if (directory.isEmpty()) GTEST_SKIP() << "set CALCULATE_SCREENSHOTS to a directory";
    QDir().mkpath(directory);
    for (const bool dark : {false, true})
        for (const bool spanish : {false, true})
            for (const QSize size : {QSize(1000, 800), QSize(390, 844)}) {
                settings::setTheme(dark ? settings::Theme::Dark : settings::Theme::Light);
                settings::setLanguage(spanish ? settings::Language::Spanish : settings::Language::English);
                MainWindow window;
                window.resize(size);
                window.show();
                ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
                const bool phone = size.width() < size.height();
                if (phone) window.findChild<QToolButton*>("panelToggle")->setChecked(true);  // the rail folded
                window.showInspector();
                auto* inspector = window.findChild<Inspector*>("inspector");
                inspector->setFormatIndex(inspector->formatIndex(calculate_core::NumberType::Float));
                inspector->setDecimal("0.1");
                QTest::qWait(100);
                const QString name = QStringLiteral("/inspector-%1-%2-%3.png")
                                         .arg(dark ? "dark" : "light", spanish ? "es" : "en", phone ? "phone" : "desktop");
                EXPECT_TRUE(window.grab().save(directory + name));
            }
    settings::setLanguage(settings::Language::English);
    settings::setTheme(settings::Theme::System);
}
