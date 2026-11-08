#include "mainwindow.hpp"

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

namespace {

QString fileText(const QString& path) {
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << path.toStdString();
    return QString::fromUtf8(file.readAll());
}

}  // namespace

// Every third-party notice and licence that travels with the program can be read in it: the screen typeface's, then
// Qt's (the LGPL-3.0 and the GPL-3.0 it adds its permissions to).
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
    ASSERT_EQ(list->count(), 3);
    EXPECT_EQ(list->currentRow(), 0);
    QFile ofl(QStringLiteral(":/fonts/JetBrainsMono-OFL.txt"));
    ASSERT_TRUE(ofl.open(QIODevice::ReadOnly));
    EXPECT_EQ(list->currentItem()->text(), "JetBrains Mono: OFL.txt");
    EXPECT_EQ(text->toPlainText(), QString::fromUtf8(ofl.readAll()));
    list->setCurrentRow(1);
    EXPECT_EQ(list->currentItem()->text(), "Qt: LGPL-3.0-only.txt");
    EXPECT_EQ(text->toPlainText(), fileText(QStringLiteral(CALCULATE_LICENSES_DIR "/LGPL-3.0-only.txt")));
    EXPECT_TRUE(text->toPlainText().startsWith(QStringLiteral("                   GNU LESSER GENERAL PUBLIC LICENSE")));
    list->setCurrentRow(2);
    EXPECT_EQ(list->currentItem()->text(), "Qt: GPL-3.0-only.txt");
    EXPECT_EQ(text->toPlainText(), fileText(QStringLiteral(CALCULATE_LICENSES_DIR "/GPL-3.0-only.txt")));
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
