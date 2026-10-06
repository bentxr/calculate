#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "formulatip.hpp"
#include "lcd.hpp"
#include "presenter.hpp"
#include "settings.hpp"
#include "typechooser.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QHelpEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>
#include <QToolTip>

#include "printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

template <class W>
W* child(MainWindow& window, const char* name) {
    W* w = window.findChild<W*>(name);
    EXPECT_NE(w, nullptr) << name;
    return w;
}

Lcd* lcd(MainWindow& window) { return child<Lcd>(window, "lcd"); }

// A choice of the settings menu, such as "theme:dark".
QAction* setting(MainWindow& window, const char* name) { return child<QAction>(window, name); }

QLabel* message(MainWindow& window) { return child<QLabel>(window, "message"); }

// Waits for the worker's answer: a value on the screen, or a message under it.
bool answered(MainWindow& window) {
    return QTest::qWaitFor([&] { return !lcd(window)->outputText().isEmpty() || !message(window)->text().isEmpty(); }, 10000);
}

// Clears the last answer, so answered() waits for the next one.
void forget(MainWindow& window) {
    lcd(window)->clearResult();
    message(window)->clear();
}

// Enters an expression, presses Enter and waits for the answer.
void run(MainWindow& window, const QString& expression) {
    forget(window);
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

}  // namespace

TEST(MainWindow, TheTypeMenuComesFromTheEngine) {
    MainWindow window;
    auto* type = child<TypeChooser>(window, "type");
    ASSERT_EQ(type->count(), 7);
    EXPECT_EQ(type->currentIndex(), 1);
    EXPECT_EQ(type->currentType(), calculate_core::NumberType::Double);
    EXPECT_EQ(type->itemText(1), "Double");
    EXPECT_EQ(type->itemText(5), "Hexadecuple");
    EXPECT_EQ(type->itemText(6), "Exact");  // the most precise of all, so the last
    for (int i = 1; i < 6; ++i)             // the others from the least precise
        EXPECT_LE(type->itemData(i - 1, TypeChooser::PrecisionRole).toInt(), type->itemData(i, TypeChooser::PrecisionRole).toInt()) << i;
    type->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_EQ(type->currentIndex(), 6);
    EXPECT_EQ(type->currentType(), calculate_core::NumberType::Exact);
    EXPECT_EQ(type->itemData(1, TypeChooser::DigitsRole).toString(), "~16 significant digits");
    EXPECT_EQ(type->itemData(1, TypeChooser::DetailRole).toString(), "double · 64-bit storage · 53-bit significand");
    EXPECT_EQ(type->itemData(1, Qt::ToolTipRole).toString(), "Double · double · 64-bit · ~16 digits");
    EXPECT_GT(type->view()->minimumWidth(), 2 * type->sizeHint().width());  // the menu is wider than the button
    // the menu looks like the Details card: window colours inside a panel frame
    EXPECT_EQ(type->view()->viewport()->backgroundRole(), QPalette::Window);
    EXPECT_EQ(type->view()->frameShape(), QFrame::StyledPanel);
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
    forget(window);
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1/3 = 0.(3)");
    EXPECT_EQ(detail(window, "exact"), "exact · no rounding error");
}

TEST(MainWindow, ExactModeGreysOutTranscendentalKeys) {
    MainWindow window;
    auto* sin = child<QPushButton>(window, "key:sin");
    EXPECT_TRUE(sin->isEnabled());
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_FALSE(sin->isEnabled());
    EXPECT_FALSE(sin->toolTip().isEmpty());
    EXPECT_TRUE(child<QPushButton>(window, "key:sqrt")->isEnabled());
    EXPECT_FALSE(child<QPushButton>(window, "direct:pi")->isEnabled());
    EXPECT_TRUE(child<QPushButton>(window, "direct:factorial")->isEnabled());
}

TEST(MainWindow, TheKeypadEditsTheExpression) {
    MainWindow window;
    QTest::mouseClick(child<QPushButton>(window, "key:sin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "direct:pi"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:close"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(π)");
    QTest::mouseClick(child<QPushButton>(window, "key:delete"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(π");
    QTest::mouseClick(child<QPushButton>(window, "key:clear"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "");
}

TEST(MainWindow, ThereIsNoShiftAlphaOrMenu) {
    MainWindow window;
    for (const char* name : {"key:shift", "key:alpha", "key:menu", "key:options", "key:on", "key:calc"})
        EXPECT_EQ(window.findChild<QPushButton*>(name), nullptr) << name;
    EXPECT_EQ(window.findChild<QMenu*>("menu"), nullptr);
    EXPECT_EQ(window.findChild<QMenu*>("options"), nullptr);
    EXPECT_EQ(window.findChild<QLabel*>("shift:sin"), nullptr);
}

TEST(MainWindow, UncertainArgumentsOfferToProceed) {
    MainWindow window;
    auto* proceed = child<QPushButton>(window, "proceed");
    run(window, "(0.1*30)!");
    EXPECT_TRUE(proceed->isVisibleTo(&window));
    forget(window);
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
    QTest::mouseClick(child<QPushButton>(window, "direct:memorySubtract"), Qt::LeftButton);
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


TEST(MainWindow, MemoryClearEmptiesTheMemory) {
    MainWindow window;
    run(window, "5");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !lcd(window)->memory().isEmpty(); }, 10000));
    QTest::mouseClick(child<QPushButton>(window, "direct:memoryClear"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory().isEmpty(); }, 10000));
}

TEST(MainWindow, CancelStaysOfferedWhileALaterRequestRuns) {
    MainWindow window;
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    lcd(window)->setInput("1 + 1");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    lcd(window)->setInput("200000!");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    auto* cancel = child<QPushButton>(window, "cancel");
    ASSERT_TRUE(QTest::qWaitFor([&] { return cancel->isVisibleTo(&window); }, 5000));
    QTest::mouseClick(cancel, Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->text() == "Cancelled"; }, 10000))
        << message(window)->text().toStdString();
    EXPECT_FALSE(cancel->isVisibleTo(&window));
}

TEST(MainWindow, KeysLeaveTheKeyboardToTheScreen) {
    MainWindow window;
    EXPECT_EQ(child<QPushButton>(window, "key:7")->focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(child<QPushButton>(window, "direct:asin")->focusPolicy(), Qt::NoFocus);
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

TEST(MainWindow, EveryRowOfTheCardExplainsItselfInAPopup) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    run(window, "0.1 + 0.2");
    QTest::mouseClick(child<QToolButton>(window, "detailsButton"), Qt::LeftButton);
    auto* card = child<DetailsCard>(window, "detailsCard");
    ASSERT_TRUE(card->isVisible());
    const QSize size = card->size();
    for (const char* key : {"bound", "measured", "trusted", "condition", "input", "rounding", "library", "operations",
                            "type", "evaluated"}) {
        auto* info = card->findChild<QToolButton*>(QString("info:") + key);
        ASSERT_NE(info, nullptr) << key;
        card->findChild<QScrollArea*>()->ensureWidgetVisible(info);  // as the user scrolls to a row
        QTest::mouseClick(info, Qt::LeftButton);
        auto* tip = card->findChild<QFrame*>("explanation");
        ASSERT_NE(tip, nullptr);
        EXPECT_TRUE(tip->isVisible()) << key;
        EXPECT_EQ(tip->findChild<QLabel*>()->text(), view::explanation(key));
        const QRect sign(info->mapToGlobal(QPoint(0, 0)), info->size());
        const QRect at = tip->geometry();
        EXPECT_TRUE(at.top() == sign.bottom() + 1 || at.bottom() + 1 == sign.top()) << key;  // under its sign, or over it
        EXPECT_EQ(card->size(), size);  // the card itself never changes
        tip->hide();
    }
    card->hide();
}

// The browser build has no font with the ⓘ character, so the sign is drawn, not typed.
TEST(MainWindow, TheInfoSignIsDrawnNotTyped) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    run(window, "0.1 + 0.2");
    QTest::mouseClick(child<QToolButton>(window, "detailsButton"), Qt::LeftButton);
    auto* card = child<DetailsCard>(window, "detailsCard");
    const QList<QToolButton*> infos = card->findChildren<QToolButton*>(QRegularExpression("^info:"));
    ASSERT_FALSE(infos.isEmpty());
    for (QToolButton* info : infos) {
        EXPECT_TRUE(info->text().isEmpty()) << info->objectName().toStdString();
        const QImage image = info->grab().toImage();
        const QColor background = image.pixelColor(0, 0);
        int ink = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAbs(image.pixelColor(x, y).lightness() - background.lightness()) > 60) ++ink;
        EXPECT_GT(ink, 10) << info->objectName().toStdString();  // a visible sign
    }
    card->hide();
}

TEST(MainWindow, LightLinesSeparateTheRowsOfTheCard) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    run(window, "0.1 + 0.2");
    QTest::mouseClick(child<QToolButton>(window, "detailsButton"), Qt::LeftButton);
    auto* card = child<DetailsCard>(window, "detailsCard");
    QList<QLabel*> labels = card->findChildren<QLabel*>(QRegularExpression("^label:"));
    std::sort(labels.begin(), labels.end(), [](QLabel* a, QLabel* b) { return a->y() < b->y(); });
    const QList<QFrame*> lines = card->findChildren<QFrame*>(QRegularExpression("^separator:"));
    ASSERT_GE(labels.size(), 2);
    EXPECT_EQ(lines.size(), labels.size() - 1);  // between rows, not around them
    const auto bottomOfRow = [&](const QString& key) {
        int bottom = 0;
        for (const char* part : {"label:", "value:", "info:"})
            bottom = qMax(bottom, card->findChild<QWidget*>(part + key)->geometry().bottom());
        return bottom;
    };
    for (int i = 0; i + 1 < labels.size(); ++i) {
        const QString key = labels[i]->objectName().mid(6);
        auto* line = card->findChild<QFrame*>("separator:" + key);
        ASSERT_NE(line, nullptr) << key.toStdString();
        EXPECT_GT(line->y(), bottomOfRow(key)) << key.toStdString();
        EXPECT_LT(line->geometry().bottom(), labels[i + 1]->y()) << key.toStdString();
        EXPECT_LE(line->x(), labels[i]->x());  // from the label to the info sign
        EXPECT_GE(line->geometry().right(), card->findChild<QWidget*>("info:" + key)->geometry().right());
        EXPECT_EQ(line->frameShape(), QFrame::HLine);
        EXPECT_EQ(line->foregroundRole(), QPalette::Mid);  // light, not the text's colour
        EXPECT_LE(line->height(), 2);
    }
    card->hide();
}

// In a small window every popup stays inside it (in the browser, what is outside cannot be reached):
// the card scrolls, and a formula never covers the pointer, which would make it flicker.
TEST(MainWindow, PopupsStayInsideASmallWindow) {
    MainWindow window;
    window.setGeometry(40, 30, 640, 420);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const QRect bounds = window.geometry();
    run(window, "1+1");
    run(window, "0.1 + 0.2");

    QTest::mouseClick(child<QToolButton>(window, "detailsButton"), Qt::LeftButton);
    auto* card = child<DetailsCard>(window, "detailsCard");
    ASSERT_TRUE(card->isVisible());
    EXPECT_TRUE(bounds.contains(card->geometry()));
    auto* rows = card->findChild<QScrollArea*>();
    ASSERT_NE(rows, nullptr);
    EXPECT_GT(rows->verticalScrollBar()->maximum(), 0);  // shorter than its rows: it scrolls
    auto* last = card->findChild<QToolButton*>("info:evaluated");
    rows->ensureWidgetVisible(last);
    QTest::mouseClick(last, Qt::LeftButton);
    auto* tip = card->findChild<QFrame*>("explanation");
    EXPECT_TRUE(tip->isVisible());
    EXPECT_TRUE(bounds.contains(tip->geometry()));
    tip->hide();
    card->hide();

    QTest::mouseClick(child<QToolButton>(window, "historyToggle"), Qt::LeftButton);
    auto* panel = child<QWidget>(window, "historyPanel");
    EXPECT_TRUE(panel->isVisible());
    EXPECT_TRUE(bounds.contains(panel->geometry()));
    panel->hide();

    child<QListWidget>(window, "modes")->setCurrentRow(1);
    QTest::qWait(50);
    auto* stdevp = child<QPushButton>(window, "stat:stdevp");
    const QPoint pointer = stdevp->mapToGlobal(QPoint(stdevp->width() - 3, stdevp->height() / 2));
    QHelpEvent hover(QEvent::ToolTip, stdevp->mapFromGlobal(pointer), pointer);
    QCoreApplication::sendEvent(stdevp, &hover);
    auto* formula = child<FormulaTip>(window, "formulaTip");
    EXPECT_TRUE(formula->isVisible());
    EXPECT_TRUE(bounds.contains(formula->geometry()));
    EXPECT_FALSE(formula->geometry().contains(pointer));
    formula->hide();
}

// Qt's own popups too: the lists of the type and angle menus, and tooltips.
TEST(MainWindow, MenusAndTooltipsStayInsideANarrowWindow) {
    MainWindow window;
    window.setGeometry(40, 30, 380, 420);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const QRect bounds = window.geometry();
    for (const char* name : {"type", "angle"}) {
        auto* menu = child<QComboBox>(window, name);
        menu->showPopup();
        QWidget* list = menu->view()->window();
        EXPECT_TRUE(list->isVisible()) << name;
        EXPECT_TRUE(bounds.contains(list->geometry())) << name;
        menu->hidePopup();
    }
    QToolTip::showText(bounds.bottomRight() - QPoint(5, 5), QString("a long explanation ").repeated(12), &window);
    QWidget* tip = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets())
        if (w->objectName() == "qtooltip_label" && w->isVisible()) tip = w;
    ASSERT_NE(tip, nullptr);
    EXPECT_TRUE(bounds.contains(tip->geometry()));  // its words wrap to the window's width
    QToolTip::hideText();
}

// With a larger font (the browser's), the type list's rows are wider than a phone: the list is cut
// to the window rather than left out of reach, and it widens again when there is room.
TEST(MainWindow, TheTypeListNarrowsToFitTheWindow) {
    MainWindow window;
    window.setGeometry(40, 30, 380, 420);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* type = child<TypeChooser>(window, "type");
    QFont big = type->view()->font();
    big.setPixelSize(30);
    type->view()->setFont(big);
    type->retranslate();
    const int wanted = type->view()->minimumWidth();
    ASSERT_GT(wanted, window.width());
    type->showPopup();
    EXPECT_TRUE(window.geometry().contains(type->view()->window()->geometry()));
    type->hidePopup();
    window.setGeometry(40, 30, 1600, 420);
    QCoreApplication::processEvents();
    type->showPopup();
    EXPECT_GE(type->view()->width(), wanted);  // the whole rows again
    type->hidePopup();
}

// Some platforms (the browser) place a menu's list again after showing it: it must stay inside then too.
TEST(MainWindow, APopupMovedAfterItShowsStaysInside) {
    MainWindow window;
    window.setGeometry(40, 30, 380, 420);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* popup = new QFrame(&window, Qt::Popup);
    popup->setGeometry(100, 100, 100, 100);
    popup->show();
    popup->setGeometry(300, 100, 600, 100);  // wider than the window, past its right edge
    QCoreApplication::processEvents();
    EXPECT_TRUE(window.geometry().contains(popup->geometry()));
    popup->hide();
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

TEST(MainWindow, KeysKeepTheirSizeAndScrollWhenTheWindowIsSmall) {
    MainWindow window;
    window.resize(900, 700);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const QSize sin = child<QPushButton>(window, "key:sin")->size();
    for (const char* key : {"key:open", "key:fraction", "key:tan", "key:memoryAdd"})
        EXPECT_EQ(child<QPushButton>(window, key)->size(), sin) << key;
    const QSize seven = child<QPushButton>(window, "key:7")->size();
    EXPECT_GT(seven.width(), sin.width());  // five number keys span the width of six function keys
    window.resize(1600, 1000);
    QTest::qWait(50);
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->size(), sin);  // bigger window: the keys stay still
    EXPECT_EQ(child<QPushButton>(window, "key:7")->size(), seven);
    auto* keys = child<QScrollArea>(window, "keys");
    EXPECT_FALSE(keys->horizontalScrollBar()->isVisible());
    window.resize(260, 400);
    QTest::qWait(50);
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->size(), sin);  // smaller window: they scroll instead
    EXPECT_TRUE(keys->horizontalScrollBar()->isVisible());
}

TEST(MainWindow, DirectKeysInsertTheirFunctions) {
    MainWindow window;
    QTest::mouseClick(child<QPushButton>(window, "direct:asin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "direct:pi"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "direct:comma"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "direct:sinh"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "asin(π, sinh(");
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_FALSE(child<QPushButton>(window, "direct:sinh")->isEnabled());
    EXPECT_FALSE(child<QPushButton>(window, "direct:sinh")->toolTip().isEmpty());
    EXPECT_TRUE(child<QPushButton>(window, "direct:mod")->isEnabled());
}

TEST(MainWindow, DirectKeysSitLeftOfTheMainPadAtTheSameSize) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    EXPECT_EQ(child<QPushButton>(window, "direct:asin")->size(), child<QPushButton>(window, "key:sin")->size());
    const QWidget* direct = child<QWidget>(window, "directKeys");
    const QWidget* keypad = child<QWidget>(window, "keypad");
    EXPECT_LT(direct->mapTo(&window, QPoint(direct->width(), 0)).x(), keypad->mapTo(&window, QPoint(0, 0)).x());
}

TEST(MainWindow, TheLeftPanelCollapsesToARail) {
    MainWindow window;
    window.resize(900, 700);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* rail = child<QWidget>(window, "rail");
    auto* toggle = child<QToolButton>(window, "panelToggle");
    auto* modes = child<QListWidget>(window, "modes");
    auto* settings = child<QToolButton>(window, "settingsButton");
    EXPECT_TRUE(modes->isVisible());
    const int wide = rail->width();
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTest::qWait(50);
    EXPECT_FALSE(modes->isVisible());
    EXPECT_LT(rail->width(), wide / 2);
    EXPECT_TRUE(toggle->isVisible());
    EXPECT_TRUE(settings->isVisible());
    EXPECT_GT(settings->y(), rail->height() / 2);  // ⚙ stays at the bottom
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTest::qWait(50);
    EXPECT_TRUE(modes->isVisible());
}

TEST(MainWindow, TheSettingsOpenAsAMenuFromTheGear) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* gear = child<QToolButton>(window, "settingsButton");
    QTest::mouseClick(gear, Qt::LeftButton);
    EXPECT_EQ(window.findChild<QDialog*>("settings"), nullptr);  // not a window of its own
    auto* menu = child<QMenu>(window, "settings");
    ASSERT_TRUE(menu->isVisible());
    const QRect button(gear->mapToGlobal(QPoint(0, 0)), gear->size());
    EXPECT_EQ(menu->geometry().left(), button.left());  // it stems from the gear, upwards
    EXPECT_EQ(menu->geometry().bottom() + 1, button.top());
    EXPECT_TRUE(window.geometry().contains(menu->geometry()));
    for (const char* name : {"language:system", "language:en", "language:es", "theme:system", "theme:light", "theme:dark"}) {
        QAction* choice = setting(window, name);
        ASSERT_NE(choice, nullptr) << name;
        EXPECT_TRUE(choice->isCheckable()) << name;  // every value listed in place
    }
    EXPECT_TRUE(setting(window, "language:system")->isChecked());
    EXPECT_TRUE(setting(window, "theme:system")->isChecked());
    setting(window, "theme:dark")->trigger();
    EXPECT_TRUE(setting(window, "theme:dark")->isChecked());
    EXPECT_FALSE(setting(window, "theme:system")->isChecked());  // one value per setting
    setting(window, "theme:system")->trigger();
    menu->close();
}

TEST(MainWindow, SpanishRelabelsEverythingWithoutARestart) {
    MainWindow window;
    run(window, "0.1 + 0.2");
    setting(window, "language:es")->trigger();
    QCoreApplication::processEvents();  // Qt posts the language change to every window
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->text(), "sen");
    EXPECT_EQ(child<QPushButton>(window, "direct:asin")->text(), "arcsen");
    EXPECT_EQ(child<QPushButton>(window, "direct:gcd")->text(), "mcd");
    EXPECT_EQ(child<QListWidget>(window, "modes")->item(0)->text(), "Calculadora");
    EXPECT_EQ(child<QComboBox>(window, "type")->itemText(1), "Doble");
    EXPECT_EQ(child<QToolButton>(window, "detailsButton")->text(), "Detalles");
    EXPECT_EQ(child<DetailsCard>(window, "detailsCard")->findChild<QLabel*>("label:bound")->text(), "Cota garantizada");
    EXPECT_EQ(setting(window, "language:system")->text(), "Sistema");
    EXPECT_EQ(setting(window, "theme:dark")->text(), "Oscuro");
    setting(window, "language:en")->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->text(), "sin");
    EXPECT_EQ(child<QListWidget>(window, "modes")->item(0)->text(), "Calculator");
}

// The browser starts dark: the menu shows the theme in use, not always System.
TEST(MainWindow, TheSettingsShowTheThemeInUse) {
    const QPalette original = QApplication::palette();
    settings::setTheme(settings::Theme::Dark);
    {
        MainWindow window;
        EXPECT_TRUE(setting(window, "theme:dark")->isChecked());
        EXPECT_FALSE(setting(window, "theme:system")->isChecked());
    }
    settings::setTheme(settings::Theme::System);
    {
        MainWindow window;
        EXPECT_TRUE(setting(window, "theme:system")->isChecked());
    }
    QApplication::setPalette(original);
}

TEST(MainWindow, TheThemeSwitchesBetweenLightAndDark) {
    const QPalette original = QApplication::palette();
    MainWindow window;
    setting(window, "theme:dark")->trigger();
    QCoreApplication::processEvents();
    EXPECT_LT(window.palette().color(QPalette::Window).lightness(), 128);
    EXPECT_LT(lcd(window)->background().lightness(), 80);
    setting(window, "theme:light")->trigger();
    QCoreApplication::processEvents();
    EXPECT_GT(window.palette().color(QPalette::Window).lightness(), 128);
    EXPECT_GT(lcd(window)->background().lightness(), 150);
    QApplication::setPalette(original);
    QCoreApplication::processEvents();
}

TEST(MainWindow, TheValuesBoxTakesOnlyNumbers) {
    MainWindow window;
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    auto* values = child<QPlainTextEdit>(window, "statisticsValues");
    QTest::keyClicks(values, "1a2;b3, 4");
    QTest::keyClick(values, Qt::Key_Return);
    QTest::keyClicks(values, "-5x");
    EXPECT_EQ(values->toPlainText(), "12;3, 4\n-5");
    QGuiApplication::clipboard()->setText("7abc8");
    QTest::keySequence(values, QKeySequence::Paste);
    EXPECT_EQ(values->toPlainText(), "12;3, 4\n-578");
}

TEST(MainWindow, AKeypadEntersValuesWithoutAKeyboard) {
    MainWindow window;
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    for (const char* key : {"statKey:7", "statKey:next", "statKey:9", "statKey:point", "statKey:5"})
        QTest::mouseClick(child<QPushButton>(window, key), Qt::LeftButton);
    EXPECT_EQ(child<QPlainTextEdit>(window, "statisticsValues")->toPlainText(), "7\n9.5");
    QTest::mouseClick(child<QPushButton>(window, "stat:mean"), Qt::LeftButton);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->input(), "mean(7, 9.5)");
    EXPECT_EQ(lcd(window)->outputText(), "8.25");
    auto* keys = child<QWidget>(window, "statisticsKeys");
    EXPECT_FALSE(keys->isHidden());
    QTest::mouseClick(child<QToolButton>(window, "statisticsKeysToggle"), Qt::LeftButton);
    EXPECT_TRUE(keys->isHidden());
}

TEST(MainWindow, TheStatisticsKeypadStaysCompact) {
    MainWindow window;
    window.resize(700, 900);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    QTest::qWait(50);
    const QWidget* seven = child<QWidget>(window, "statKey:7");
    const QWidget* four = child<QWidget>(window, "statKey:4");
    EXPECT_LT(four->y() - seven->y(), 2 * seven->height());  // rows stay together however tall the page is
}

TEST(MainWindow, ChangingTheLanguageMovesNothing) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const auto geometry = [&] {
        QList<QRect> rects;
        for (const char* name : {"lcd", "type", "angle", "equals", "key:sin", "key:7", "direct:asin", "direct:acosh", "keys"})
            rects << child<QWidget>(window, name)->geometry();
        return rects;
    };
    const QList<QRect> english = geometry();
    setting(window, "language:es")->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(geometry(), english);
    setting(window, "language:en")->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(geometry(), english);
}

TEST(MainWindow, StatisticsFunctionsSitAboveTheNumberPad) {
    MainWindow window;
    window.resize(900, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    QTest::qWait(50);
    const auto rect = [&](const char* name) {
        QWidget* w = child<QWidget>(window, name);
        return QRect(w->mapTo(&window, QPoint(0, 0)), w->size());
    };
    const QRect values = rect("statisticsValues");
    const QRect seven = rect("statKey:7");
    for (const char* name : {"stat:mean", "stat:median", "stat:var", "stat:stdev", "stat:varp", "stat:stdevp"}) {
        EXPECT_LT(rect(name).bottom(), seven.top()) << name;   // the functions over the numbers
        EXPECT_GT(rect(name).left(), values.right()) << name;  // beside the values, with the keypad
    }
}

TEST(MainWindow, StatisticsButtonsShowTheirFormula) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    child<QListWidget>(window, "modes")->setCurrentRow(1);
    auto* tip = child<FormulaTip>(window, "formulaTip");
    EXPECT_FALSE(tip->isVisible());
    for (const char* f : {"mean", "median", "var", "stdev", "varp", "stdevp"}) {
        auto* button = child<QPushButton>(window, (QString("stat:") + f).toUtf8().constData());
        QHelpEvent hover(QEvent::ToolTip, QPoint(5, 5), button->mapToGlobal(QPoint(5, 5)));
        QCoreApplication::sendEvent(button, &hover);
        EXPECT_TRUE(tip->isVisible()) << f;
        EXPECT_EQ(tip->function(), f);
        EXPECT_EQ(tip->algorithm(), view::algorithm(f));
        EXPECT_GT(tip->formulaHeight(), 1.5 * tip->fontMetrics().height()) << f;  // drawn in two dimensions
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(button, &leave);
        EXPECT_FALSE(tip->isVisible()) << f;
    }
}

TEST(MainWindow, ErrorsAppearUnderTheScreenWithoutMovingAnything) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const auto geometry = [&] {
        QList<QRect> rects;
        for (const char* name : {"lcd", "keys", "message"}) rects << child<QWidget>(window, name)->geometry();
        return rects;
    };
    const QList<QRect> before = geometry();
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    run(window, "ln(2)");
    EXPECT_TRUE(message(window)->text().startsWith("Exact arithmetic cannot represent ln")) << message(window)->text().toStdString();
    EXPECT_EQ(lcd(window)->outputText(), "");  // the screen shows values only
    EXPECT_EQ(geometry(), before);             // the strip under the screen was already there
    const auto top = [&](QWidget* w) { return w->mapTo(&window, QPoint(0, 0)).y(); };
    QWidget* screen = child<QWidget>(window, "lcd");
    EXPECT_GE(top(message(window)), top(screen) + screen->height());                                       // under the screen
    EXPECT_LE(top(message(window)) + message(window)->height(), top(child<QWidget>(window, "keys")));  // above the keys
    run(window, "1+1");
    EXPECT_EQ(message(window)->text(), "");
    EXPECT_EQ(geometry(), before);
}

TEST(MainWindow, TemplateKeysBuildTwoDimensionalInput) {
    MainWindow window;
    const auto click = [&](std::initializer_list<const char*> names) {
        for (const char* name : names) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    };
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    click({"key:fraction", "key:1", "key:down", "key:3", "key:right", "key:plus", "key:sqrt", "key:4", "key:right",
           "direct:cube"});
    EXPECT_EQ(lcd(window)->input(), "((1)/(3))+√(4)^(3)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "25/3 = 8.(3)");  // 1/3 + 2³
    lcd(window)->clear();
    click({"key:2", "key:square", "key:plus", "key:5", "key:reciprocal"});
    EXPECT_EQ(lcd(window)->input(), "2^(2)+5^(−1)");
}

TEST(MainWindow, TheHistoryBringsBackTheTwoDimensionalInput) {
    MainWindow window;
    for (const char* name : {"key:fraction", "key:1", "key:down", "key:3", "key:right"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    lcd(window)->clear();
    QTest::keyClick(lcd(window), Qt::Key_Up);
    EXPECT_EQ(lcd(window)->input(), "((1)/(3))");
    ASSERT_EQ(lcd(window)->entry().root().size(), 1u);
    EXPECT_EQ(lcd(window)->entry().root()[0].kind, Template::Fraction);  // a fraction again, not text
}

TEST(MainWindow, AJumpWithinTheErrorOffersToProceed) {
    MainWindow window;
    auto* proceed = child<QPushButton>(window, "proceed");
    run(window, "mod(0.7+0.1, 0.8)");
    EXPECT_TRUE(proceed->isVisibleTo(&window));
    forget(window);
    QTest::mouseClick(proceed, Qt::LeftButton);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(detail(window, "incomplete"), "an uncertain argument was accepted");
}

TEST(MainWindow, AnEdgeWithinTheErrorOffersToProceed) {
    MainWindow window;
    run(window, "1/(0.1+0.2-0.3)");
    EXPECT_TRUE(child<QPushButton>(window, "proceed")->isVisibleTo(&window));
}

TEST(MainWindow, TypedFunctionsEvaluate) {
    MainWindow window;
    forget(window);
    QTest::keyClicks(lcd(window), "sqrt(16)+sin(0)");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "4");
}

TEST(MainWindow, AKeyboardButtonTurnsTheSystemKeyboardOnAndOff) {
    MainWindow window;
    auto* button = child<QToolButton>(window, "keyboardButton");
    EXPECT_TRUE(button->isCheckable());
    EXPECT_EQ(button->focusPolicy(), Qt::NoFocus);
    EXPECT_FALSE(button->accessibleName().isEmpty());
    EXPECT_EQ(button->isChecked(), lcd(window)->testAttribute(Qt::WA_InputMethodEnabled));
    button->click();
    EXPECT_EQ(button->isChecked(), lcd(window)->testAttribute(Qt::WA_InputMethodEnabled));
    button->click();
    EXPECT_EQ(button->isChecked(), lcd(window)->testAttribute(Qt::WA_InputMethodEnabled));
}

TEST(MainWindow, TheEditButtonReachesEveryEditAction) {
    MainWindow window;
    auto* edit = child<QToolButton>(window, "editButton");
    EXPECT_EQ(edit->focusPolicy(), Qt::NoFocus);
    EXPECT_FALSE(edit->accessibleName().isEmpty());
    QGuiApplication::clipboard()->setText("sqrt(9)");
    child<QAction>(window, "edit:paste")->trigger();
    EXPECT_EQ(lcd(window)->input(), "√(9)");
    child<QAction>(window, "edit:undo")->trigger();
    EXPECT_EQ(lcd(window)->input(), "");
    child<QAction>(window, "edit:redo")->trigger();
    EXPECT_EQ(lcd(window)->input(), "√(9)");
}

TEST(MainWindow, AfterEqualsADigitStartsAfreshAndAnOperatorGoesOnFromAns) {
    MainWindow window;
    run(window, "2+3");
    QTest::keyClicks(lcd(window), "7");
    EXPECT_EQ(lcd(window)->input(), "7");
    run(window, "2+3");
    QTest::keyClicks(lcd(window), "*2");
    EXPECT_EQ(lcd(window)->input(), "Ans×2");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "10");
    run(window, "2+3");
    QTest::keyClick(lcd(window), Qt::Key_Left);  // an arrow edits the expression instead
    QTest::keyClicks(lcd(window), "0");
    EXPECT_EQ(lcd(window)->input(), "2+03");
    run(window, "2+3");
    QTest::mouseClick(child<QPushButton>(window, "key:square"), Qt::LeftButton);  // x² goes on from Ans too
    EXPECT_EQ(lcd(window)->input(), "Ans^(2)");
}

TEST(MainWindow, PageUpAndDownBrowseTheHistoryAndKeepTheUnfinishedInput) {
    MainWindow window;
    run(window, "1+1");
    run(window, "2+2");
    lcd(window)->clear();
    QTest::keyClicks(lcd(window), "9-");
    QTest::keyClick(lcd(window), Qt::Key_PageUp);
    EXPECT_EQ(lcd(window)->input(), "2+2");
    QTest::keyClick(lcd(window), Qt::Key_PageUp);
    EXPECT_EQ(lcd(window)->input(), "1+1");
    QTest::keyClick(lcd(window), Qt::Key_PageDown);
    QTest::keyClick(lcd(window), Qt::Key_PageDown);  // past the newest: back to what was being typed
    EXPECT_EQ(lcd(window)->input(), "9−");
}
