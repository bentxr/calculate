#include "mainwindow.hpp"

#include "lcd.hpp"
#include "typechooser.hpp"

#include <QAction>
#include <QComboBox>
#include <QInputMethodQueryEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>
#include <QTranslator>

#include "printers.hpp"

#include <gtest/gtest.h>

namespace {

template <class W>
W* child(MainWindow& window, const char* name) {
    W* w = window.findChild<W*>(name);
    EXPECT_NE(w, nullptr) << name;
    return w;
}

constexpr int plotMode = 4;

void showWide(MainWindow& window) {
    window.resize(1280, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(1280, 800));
    QApplication::processEvents();
}

void showPhone(MainWindow& window) {
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    QApplication::processEvents();
}

void choose(MainWindow& window, int mode) {
    child<QListWidget>(window, "modes")->setCurrentRow(mode);
    QTest::qWait(50);  // (the layouts settle; the plot page's scroll area lays it out)
}

// The widget's rectangle in the window's coordinates.
QRect placed(QWidget* w) { return QRect(w->mapTo(w->window(), QPoint(0, 0)), w->size()); }

}  // namespace

TEST(PlotMode, IsTheFifthMode) {
    MainWindow window;
    auto* modes = child<QListWidget>(window, "modes");
    ASSERT_EQ(modes->count(), 5);
    EXPECT_EQ(modes->item(plotMode)->text(), "Plot");
    EXPECT_EQ(child<QAction>(window, "mode:4")->text(), "Plot");
    choose(window, plotMode);
    EXPECT_TRUE(child<QStackedWidget>(window, "pages")->currentWidget()->isAncestorOf(child<QWidget>(window, "plotPage")));
}

// The plot section has no calculator screen: the screen, = and the message strip give their room to the plot, and
// come back on leaving it.
TEST(PlotMode, HidesTheScreenEqualsAndTheMessages) {
    MainWindow window;
    showWide(window);
    QWidget* screen = child<Lcd>(window, "lcd");
    QWidget* equals = child<QPushButton>(window, "equals");
    QWidget* strip = child<QWidget>(window, "messageStrip");
    choose(window, plotMode);
    EXPECT_FALSE(screen->isVisible());
    EXPECT_FALSE(equals->isVisible());
    EXPECT_FALSE(strip->isVisible());
    QWidget* page = child<QWidget>(window, "plotPage");
    EXPECT_LT(placed(page).top(), 40);  // the page starts at the window's top
    choose(window, 0);
    EXPECT_TRUE(screen->isVisible());
    EXPECT_TRUE(equals->isVisible());
    EXPECT_TRUE(strip->isVisible());
}

// Wide: the angle unit, the number type and the plot's own Details sit in the plot page's context bar, and the angle
// and type go back beside the screen on leaving. They are the same widgets, so the setting is one.
TEST(PlotMode, TheAngleAndTypeMoveIntoTheContextBarAndBack) {
    MainWindow window;
    showWide(window);
    auto* angle = child<QComboBox>(window, "angle");
    auto* type = child<TypeChooser>(window, "type");
    auto* bar = child<QWidget>(window, "plotContext");
    auto* details = child<QToolButton>(window, "plotDetails");
    EXPECT_FALSE(details->isVisible());
    choose(window, plotMode);
    EXPECT_TRUE(bar->isAncestorOf(angle));
    EXPECT_TRUE(bar->isAncestorOf(type));
    EXPECT_TRUE(bar->isAncestorOf(details));
    EXPECT_TRUE(angle->isVisible());
    EXPECT_TRUE(type->isVisible());
    EXPECT_TRUE(details->isVisible());
    EXPECT_EQ(placed(angle).top(), placed(details).top());  // one line
    EXPECT_LT(placed(angle).right(), placed(type).left());
    EXPECT_LT(placed(type).right(), placed(details).left());
    EXPECT_EQ(details->text(), "Details");
    EXPECT_FALSE(details->isEnabled());  // no series computed yet: nothing to tell
    angle->setCurrentIndex(1);  // DEG, chosen in Plot
    choose(window, 0);
    EXPECT_FALSE(bar->isAncestorOf(angle));
    EXPECT_FALSE(details->isVisible());
    QWidget* screen = child<Lcd>(window, "lcd");
    EXPECT_GT(placed(angle).left(), placed(screen).right());  // beside the screen again
    EXPECT_EQ(placed(angle).top(), placed(screen).top());
    EXPECT_EQ(angle->currentIndex(), 1);
    EXPECT_EQ(angle->width(), type->width());
    EXPECT_EQ(angle->width(), child<QPushButton>(window, "equals")->width());
}

// Phone: the top row keeps the menu, the angle, the type and the gear; the plot's Details takes the place of =.
TEST(PlotMode, OnAPhoneDetailsTakesThePlaceOfEquals) {
    MainWindow window;
    showPhone(window);
    auto* angle = child<QComboBox>(window, "angle");
    auto* type = child<TypeChooser>(window, "type");
    auto* equals = child<QPushButton>(window, "equals");
    auto* details = child<QToolButton>(window, "plotDetails");
    auto* gear = child<QToolButton>(window, "settingsButton");
    const QRect equalsPlace = placed(equals);
    const int width = window.width();
    choose(window, plotMode);
    QApplication::processEvents();
    EXPECT_EQ(window.width(), width);  // nothing in Plot is wider than the calculator
    EXPECT_FALSE(equals->isVisible());
    EXPECT_FALSE(child<Lcd>(window, "lcd")->isVisible());
    EXPECT_TRUE(details->isVisible());
    EXPECT_LT(placed(details).top(), 40);  // the top row
    EXPECT_EQ(placed(details).top(), placed(angle).top());
    EXPECT_LT(placed(angle).right(), placed(type).left());
    EXPECT_LT(placed(type).right(), placed(details).left());
    EXPECT_LT(placed(details).right(), placed(gear).left());
    EXPECT_EQ(placed(details).height(), equalsPlace.height());
    choose(window, 0);
    EXPECT_TRUE(equals->isVisible());
    EXPECT_FALSE(details->isVisible());
    EXPECT_EQ(window.width(), width);  // (nothing widened the window on the way back)
    EXPECT_EQ(placed(equals), equalsPlace);
}

// A change of arrangement while in Plot (a window made narrow, then wide) keeps each widget where that arrangement
// wants it.
TEST(PlotMode, FollowsAChangeOfArrangement) {
    MainWindow window;
    showWide(window);
    auto* angle = child<QComboBox>(window, "angle");
    auto* bar = child<QWidget>(window, "plotContext");
    auto* details = child<QToolButton>(window, "plotDetails");
    choose(window, plotMode);
    window.resize(390, 844);
    window.layOutKeys(QSize(390, 844));
    QApplication::processEvents();
    EXPECT_FALSE(bar->isAncestorOf(angle));
    EXPECT_TRUE(details->isVisible());
    EXPECT_FALSE(child<QPushButton>(window, "equals")->isVisible());
    EXPECT_FALSE(child<Lcd>(window, "lcd")->isVisible());
    window.resize(1280, 800);
    window.layOutKeys(QSize(1280, 800));
    QApplication::processEvents();
    EXPECT_TRUE(bar->isAncestorOf(angle));
    EXPECT_TRUE(bar->isAncestorOf(details));
    choose(window, 0);
    EXPECT_TRUE(child<Lcd>(window, "lcd")->isVisible());
    EXPECT_FALSE(bar->isAncestorOf(angle));
}

TEST(PlotMode, ReadsInSpanish) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    MainWindow window;
    QCoreApplication::installTranslator(&spanish);
    QApplication::processEvents();
    EXPECT_EQ(child<QListWidget>(window, "modes")->item(plotMode)->text(), QStringLiteral("Gráficas"));
    EXPECT_EQ(child<QAction>(window, "mode:4")->text(), QStringLiteral("Gráficas"));
    EXPECT_EQ(child<QToolButton>(window, "plotDetails")->text(), "Detalles");
    QCoreApplication::removeTranslator(&spanish);
}

namespace {

// Every text the plot page shows a person, by the widget (or action) that shows it: names, tooltips, placeholders,
// tab titles. Values from the figure (its titles, the series' labels, numbers) and the library's own names (style
// names; the keywords of a call, labelled "name:...", and matplotlib's description of their values) are not
// interface text.
QMap<QString, QString> pageTexts(QWidget* page) {
    QMap<QString, QString> texts;
    const auto put = [&texts](const QObject* o, const QString& what, const QString& text) {
        if (!text.trimmed().isEmpty()) texts.insert(o->objectName() + "/" + what + "@" + QString::number(quintptr(o), 16), text);
    };
    for (QObject* o : page->findChildren<QObject*>()) {
        if (o->objectName().startsWith("name:")) continue;
        // Qt's own buttons of a tab bar (their texts are Qt's translations, not the app's)
        if (o->objectName() == "ScrollLeftButton" || o->objectName() == "ScrollRightButton") continue;
        if (auto* w = qobject_cast<QWidget*>(o)) {
            put(w, "tip", w->toolTip());
            put(w, "accessible", w->accessibleName());
        }
        if (auto* b = qobject_cast<QAbstractButton*>(o)) put(b, "text", b->text());
        if (auto* l = qobject_cast<QLabel*>(o)) put(l, "text", l->text());
        if (auto* e = qobject_cast<QLineEdit*>(o)) put(e, "placeholder", e->placeholderText());
        if (auto* a = qobject_cast<QAction*>(o)) put(a, "text", a->text());
        if (auto* t = qobject_cast<QTabWidget*>(o))
            for (int i = 0; i < t->count(); ++i) put(t, "tab" + QString::number(i), t->tabText(i));
    }
    return texts;
}

}  // namespace

// In Spanish every interface text of the plot page changes (all are translated, and all are set again on a change
// of language), except words that are the same in both languages.
TEST(PlotMode, EveryTextOfThePlotPageReadsInSpanish) {
    MainWindow window;
    showWide(window);
    choose(window, plotMode);
    auto* page = child<QWidget>(window, "plotPage");
    // open what is made on first use: every section and its More panel
    for (QToolButton* b : page->findChildren<QToolButton*>())
        if (b->objectName().startsWith("section:") || b->objectName().startsWith("more:")) b->click();
    QApplication::processEvents();
    const QMap<QString, QString> english = pageTexts(page);
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    QCoreApplication::installTranslator(&spanish);
    QApplication::processEvents();
    const QMap<QString, QString> inSpanish = pageTexts(page);
    QCoreApplication::removeTranslator(&spanish);
    const QStringList same{"RAD", "DEG", "GRAD", "Double", "Float", "Long double", "Exact", "default", "…", "..."};
    int checked = 0;
    for (auto it = english.begin(); it != english.end(); ++it) {
        if (same.contains(it.value())) continue;
        ++checked;
        EXPECT_NE(inSpanish.value(it.key()), it.value()) << it.key().toStdString() << ": \"" << it.value().toStdString() << "\"";
    }
    EXPECT_GT(checked, 40);
}

namespace {

// What the platform asks a box before it brings the device's keyboard up.
bool takesAnInputMethod(QWidget* widget) {
    QInputMethodQueryEvent query(Qt::ImEnabled);
    QCoreApplication::sendEvent(widget, &query);
    return query.value(Qt::ImEnabled).toBool();
}

}  // namespace

// In the browser the device keyboard waits for the Keyboard button, so that it does not cover the keys. Plot has no
// keys, and its fields (titles, labels, the search, the data) need typing: while Plot is the mode the device keyboard
// is allowed; back in another mode the button decides again.
TEST(PlotMode, AllowsTheDeviceKeyboardForItsFields) {
    MainWindow window;
    showPhone(window);
    auto* button = child<QToolButton>(window, "keyboardButton");
    button->setChecked(false);  // the browser's start
    auto* title = child<QLineEdit>(window, "edit:title");
    auto* entry = child<Lcd>(window, "lcd");
    EXPECT_FALSE(takesAnInputMethod(entry));
    choose(window, plotMode);
    EXPECT_TRUE(takesAnInputMethod(title));
    EXPECT_TRUE(takesAnInputMethod(child<QLineEdit>(window, "plotSearch")));
    choose(window, 0);
    EXPECT_FALSE(takesAnInputMethod(entry));
    EXPECT_FALSE(button->isChecked());
    button->setChecked(true);
    choose(window, plotMode);
    choose(window, 0);
    EXPECT_TRUE(takesAnInputMethod(entry));  // the button's choice, kept
}
