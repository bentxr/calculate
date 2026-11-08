#include "mainwindow.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

// The licence files of the components the program ships, as the licensing manifest lists them: each once, in
// path order (test-only components, such as GoogleTest, are not shipped).
QStringList shippedLicenceFiles() {
    QFile file(QStringLiteral(CALCULATE_SOURCE_DIR "/licensing/manifest.json"));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    QStringList paths;
    for (const QJsonValue component : QJsonDocument::fromJson(file.readAll()).object().value("components").toArray()) {
        if (component["use"].toString() != "runtime") continue;
        for (const QJsonValue licence : component["licenceFiles"].toArray())
            if (!paths.contains(licence["path"].toString())) paths << licence["path"].toString();
    }
    paths.sort();
    return paths;
}

}  // namespace

// Every third-party notice and licence that travels with the program can be read in it: NOTICE first (Qt, the
// third-party code inside Qt, the typeface, the engine's libraries), then the licence of each component it names,
// in path order.
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
    const QStringList shipped = QStringList{"NOTICE"} + shippedLicenceFiles();
    ASSERT_GT(shipped.size(), 30);
    ASSERT_EQ(list->count(), shipped.size());
    EXPECT_EQ(list->currentRow(), 0);
    for (int i = 0; i < shipped.size(); ++i) {
        list->setCurrentRow(i);
        EXPECT_EQ(list->currentItem()->text(), shipped[i]);
        EXPECT_EQ(text->toPlainText(), fileText(QStringLiteral(CALCULATE_SOURCE_DIR "/") + shipped[i]))
            << shipped[i].toStdString();
    }
    list->setCurrentRow(0);
    EXPECT_TRUE(text->toPlainText().contains(QStringLiteral("Qt 6.11.2")));
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

// The browser build links Qt statically, so the LGPL-3.0 asks that a user can rebuild it against a modified Qt and
// may debug such a change: NOTICE (shown in the view) says how and grants it, and the README points to it.
TEST(Licences, TheNoticeSaysHowToUseAModifiedQt) {
    const QString notice = fileText(QStringLiteral(CALCULATE_SOURCE_DIR "/NOTICE"));
    EXPECT_TRUE(notice.contains(QStringLiteral("Using a modified Qt")));
    EXPECT_TRUE(notice.contains(QStringLiteral("https://github.com/bentxr/calculate")));
    EXPECT_TRUE(notice.contains(QStringLiteral("cmake --preset wasm-ci && cmake --build --preset wasm-ci")));
    EXPECT_TRUE(notice.contains(QStringLiteral("reverse engineering")));
    EXPECT_TRUE(fileText(QStringLiteral(CALCULATE_SOURCE_DIR "/README.md")).contains(QStringLiteral("NOTICE")));
}
