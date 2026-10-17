#include "detailscard.hpp"
#include "inspector.hpp"
#include "lcd.hpp"
#include "mainwindow.hpp"
#include "plotstyles.hpp"
#include "settings.hpp"

#include <QDir>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
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

TEST(Screenshots, Plot) {
    const QString directory = qEnvironmentVariable("CALCULATE_SCREENSHOTS");
    if (directory.isEmpty()) GTEST_SKIP() << "set CALCULATE_SCREENSHOTS to a directory";
    QDir().mkpath(directory);
    for (const bool dark : {false, true})
        for (const QSize size : {QSize(1280, 800), QSize(390, 844)}) {
            settings::setTheme(dark ? settings::Theme::Dark : settings::Theme::Light);
            MainWindow window;
            window.resize(size);
            window.show();
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
            window.layOutKeys(size);
            window.findChild<QListWidget*>("modes")->setCurrentRow(4);
            QTest::qWait(1500);  // the canvas' first drawing (the fonts are read once)
            const bool phone = size.width() < size.height();
            const QString name = QStringLiteral("/plot-%1-%2.png").arg(dark ? "dark" : "light", phone ? "phone" : "desktop");
            EXPECT_TRUE(window.grab().save(directory + name));
            if (phone) {  // the editor's sheet open over the canvas
                window.findChild<QPushButton*>("sheet:style")->click();
                QTest::qWait(200);
                EXPECT_TRUE(window.grab().save(directory + QStringLiteral("/plot-%1-phone-sheet.png").arg(dark ? "dark" : "light")));
            }
            if (!phone && dark) {  // the style chooser, its thumbnails drawn
                PlotStylesDialog styles(QByteArray(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
                                                   R"( {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]], "kwargs": {"marker": "o"}},)"
                                                   R"( {"on": "ax", "call": "set_title", "args": ["Squares"]}]})"),
                                        &window);
                styles.show();
                ASSERT_TRUE(QTest::qWaitFor([&] { return styles.finished(); }, 60000));
                EXPECT_TRUE(styles.grab().save(directory + "/plot-styles.png"));
            }
        }
    // in Spanish: the wide page with every section open, the phone with the sheet open
    settings::setLanguage(settings::Language::Spanish);
    settings::setTheme(settings::Theme::Light);
    for (const QSize size : {QSize(1280, 800), QSize(390, 844)}) {
        MainWindow window;
        window.resize(size);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        window.layOutKeys(size);
        window.findChild<QListWidget*>("modes")->setCurrentRow(4);
        QTest::qWait(1500);
        const bool phone = size.width() < size.height();
        if (phone) window.findChild<QPushButton*>("sheet:style")->click();
        QTest::qWait(200);
        EXPECT_TRUE(window.grab().save(directory + (phone ? "/plot-es-phone-sheet.png" : "/plot-es-desktop.png")));
    }
    settings::setLanguage(settings::Language::English);
    settings::setTheme(settings::Theme::System);
}
