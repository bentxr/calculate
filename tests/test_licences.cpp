#include "mainwindow.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTest>
#include <QTranslator>

#include "printers.hpp"

#include <gtest/gtest.h>

namespace {

QDialog* openLicences(MainWindow& window) {
    QAction* action = window.findChild<QAction*>("licences");
    EXPECT_NE(action, nullptr);
    if (!action) return nullptr;
    action->trigger();
    QApplication::processEvents();
    return window.findChild<QDialog*>("licencesDialog");
}

}  // namespace

// Every third-party notice and licence that travels with the program can be read in it: the plotting library's
// (its NOTICE, then each component's licence) and the screen typeface's.
TEST(Licences, TheViewShowsEveryThirdPartyText) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    QDialog* dialog = openLicences(window);
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    auto* list = dialog->findChild<QListWidget*>("licenceList");
    auto* text = dialog->findChild<QPlainTextEdit*>("licenceText");
    ASSERT_NE(list, nullptr);
    ASSERT_NE(text, nullptr);
    EXPECT_TRUE(text->isReadOnly());
    const std::vector<calculate_plot::LicenceText> plot = calculate_plot::licenceTexts();
    ASSERT_GT(plot.size(), 10u);
    ASSERT_EQ(list->count(), static_cast<int>(plot.size()) + 1);
    EXPECT_EQ(list->currentRow(), 0);  // the plotting library's NOTICE first
    for (std::size_t i = 0; i < plot.size(); ++i) {
        list->setCurrentRow(static_cast<int>(i));
        EXPECT_EQ(list->item(static_cast<int>(i))->text(),
                  QStringLiteral("calculate-plot: ") + QString::fromUtf8(plot[i].path.data(), static_cast<qsizetype>(plot[i].path.size())));
        EXPECT_EQ(text->toPlainText(), QString::fromUtf8(plot[i].text.data(), static_cast<qsizetype>(plot[i].text.size())));
    }
    QFile ofl(QStringLiteral(":/fonts/JetBrainsMono-OFL.txt"));
    ASSERT_TRUE(ofl.open(QIODevice::ReadOnly));
    list->setCurrentRow(list->count() - 1);
    EXPECT_EQ(list->currentItem()->text(), "JetBrains Mono: OFL.txt");
    EXPECT_EQ(text->toPlainText(), QString::fromUtf8(ofl.readAll()));
    EXPECT_TRUE(window.geometry().contains(dialog->geometry()));
    dialog->close();
}

// It is not a setting: the settings file has no key for it.
TEST(Licences, AreNotASetting) {
    MainWindow window;
    EXPECT_FALSE(window.settingKeys().contains("licences"));
}

TEST(Licences, ReadInSpanish) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    MainWindow window;
    QCoreApplication::installTranslator(&spanish);
    QApplication::processEvents();
    EXPECT_EQ(window.findChild<QAction*>("licences")->text(), QStringLiteral("Licencias de terceros…"));
    QDialog* dialog = openLicences(window);
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Licencias de terceros"));
    dialog->close();
    QCoreApplication::removeTranslator(&spanish);
}
