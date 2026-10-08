#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "formulatip.hpp"
#include "keybutton.hpp"
#include "keypad.hpp"
#include "keysizing.hpp"
#include "lcd.hpp"
#include "presenter.hpp"
#include "settings.hpp"
#include "typechooser.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
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

// Opens a section of the left column, as a click on its header does (sections start closed).
void openSection(MainWindow& window, const QString& id) {
    auto* header = window.findChild<QToolButton*>("section:" + id);
    ASSERT_NE(header, nullptr) << id.toStdString();
    if (!header->isChecked()) QTest::mouseClick(header, Qt::LeftButton);
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
    QTest::mouseClick(child<QPushButton>(window, "common:pi"), Qt::LeftButton);
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
    QTest::mouseClick(child<QPushButton>(window, "common:asin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "common:pi"), Qt::LeftButton);
    openSection(window, "numbers");
    QTest::mouseClick(child<QPushButton>(window, "direct:comma"), Qt::LeftButton);
    openSection(window, "hyperbolic");
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
    EXPECT_GT(settings->y(), rail->height() / 2);  // the gear stays at the bottom
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
           "common:cube"});
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

TEST(MainWindow, SymbolButtonsHaveSpokenNames) {
    MainWindow window;
    for (const char* name : {"panelToggle", "settingsButton", "historyToggle"})
        EXPECT_FALSE(child<QToolButton>(window, name)->accessibleName().isEmpty()) << name;
}

TEST(MainWindow, TheSettingsOfferCalculatingAsYouType) {
    MainWindow window;
    QAction* live = setting(window, "live");
    ASSERT_NE(live, nullptr);
    EXPECT_TRUE(live->isCheckable());
    EXPECT_TRUE(live->isChecked());  // on by default
    live->trigger();
    EXPECT_FALSE(settings::liveCalculation());
    live->trigger();
    EXPECT_TRUE(settings::liveCalculation());
}

TEST(MainWindow, TheResultFollowsTheTyping) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "1+2");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "3"; }, 10000));
    EXPECT_TRUE(lcd(window)->provisional());
    EXPECT_EQ(child<QListWidget>(window, "history")->count(), 0);  // nothing committed yet
    QTest::keyClicks(lcd(window), "*4");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "9"; }, 10000));
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(QTest::qWaitFor([&] { return !lcd(window)->provisional(); }, 10000));
    EXPECT_EQ(lcd(window)->outputText(), "9");
    EXPECT_EQ(child<QListWidget>(window, "history")->count(), 1);
}

TEST(MainWindow, WithoutLiveCalculationOnlyEqualsCalculates) {
    MainWindow window;
    setting(window, "live")->trigger();  // off
    QTest::keyClicks(lcd(window), "1+2");
    QTest::qWait(800);
    EXPECT_EQ(lcd(window)->outputText(), "");
    setting(window, "live")->trigger();  // on again for the other tests
}

TEST(MainWindow, UndoAndRedoRecalculateWhatIsBeingTyped) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "1+2*4");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "9"; }, 10000));
    QTest::keyClick(lcd(window), Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(lcd(window), Qt::Key_Z, Qt::ControlModifier);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "3"; }, 10000));
    QTest::keyClick(lcd(window), Qt::Key_Y, Qt::ControlModifier);
    QTest::keyClick(lcd(window), Qt::Key_Y, Qt::ControlModifier);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "9"; }, 10000));
}

TEST(MainWindow, AnUnfinishedExpressionShowsNoErrorAndOthersAreDimmed) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "2*");
    QTest::qWait(800);  // well past the typing delay
    EXPECT_EQ(message(window)->text(), "");
    EXPECT_EQ(lcd(window)->outputText(), "");
    QTest::keyClicks(lcd(window), "1/0");
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->text() == "Division by zero"; }, 10000));
    EXPECT_EQ(message(window)->foregroundRole(), QPalette::PlaceholderText);  // dimmed while typing
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->foregroundRole() == QPalette::WindowText; }, 10000));
    EXPECT_EQ(message(window)->text(), "Division by zero");
}

TEST(MainWindow, ALongCalculationIsLeftForEquals) {
    MainWindow window;
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    QTest::keyClicks(lcd(window), "200000!");
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->text().startsWith("Too long"); }, 10000))
        << message(window)->text().toStdString();
    EXPECT_FALSE(child<QPushButton>(window, "cancel")->isVisibleTo(&window));  // nothing to cancel: it was dropped
}

TEST(MainWindow, ATemplateWithAnEmptyBoxShowsNoError) {
    MainWindow window;
    lcd(window)->insertTemplate(Template::Fraction);
    lcd(window)->insert("1");
    lcd(window)->right();  // into the empty denominator: the engine reads an unexpected ")"
    QTest::qWait(800);
    EXPECT_EQ(message(window)->text(), "");
    lcd(window)->insert("4");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "0.25"; }, 10000));
}

TEST(MainWindow, ChangingTheTypeRecalculatesWhatIsBeingTyped) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "1/3");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText().startsWith("0.33"); }, 10000));
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "1/3 = 0.(3)"; }, 10000));
    EXPECT_TRUE(lcd(window)->provisional());
    EXPECT_EQ(child<QListWidget>(window, "history")->count(), 0);
}

TEST(MainWindow, AReplayedEntryIsPreviewed) {
    MainWindow window;
    run(window, "2+2");
    run(window, "5");
    lcd(window)->clear();
    QTest::keyClick(lcd(window), Qt::Key_Up);
    QTest::keyClick(lcd(window), Qt::Key_Up);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "4" && lcd(window)->provisional(); }, 10000));
}

TEST(MainWindow, AnEntryReplayedAfterAPauseIsPreviewed) {
    MainWindow window;
    run(window, "2+2");
    run(window, "5");
    lcd(window)->clear();
    QTest::qWait(800);  // the clearing's own preview is long done
    QTest::keyClick(lcd(window), Qt::Key_Up);
    QTest::keyClick(lcd(window), Qt::Key_Up);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "4" && lcd(window)->provisional(); }, 10000));
}

TEST(MainWindow, TheFaultyPartOfTheInputIsMarked) {
    MainWindow window;
    run(window, "1+2÷0");
    EXPECT_EQ(lcd(window)->markedText(), "2÷0");
    run(window, "1+1");
    EXPECT_EQ(lcd(window)->markedText(), "");
}

TEST(MainWindow, DetailsGoesOffWhenThePreviewGoes) {
    MainWindow window;
    auto* details = child<QToolButton>(window, "detailsButton");
    QTest::keyClicks(lcd(window), "1+2");
    ASSERT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "3"; }, 10000));
    EXPECT_TRUE(details->isEnabled());
    lcd(window)->clear();  // an empty input: nothing to describe
    EXPECT_TRUE(QTest::qWaitFor([&] { return !details->isEnabled(); }, 10000));

    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    QTest::keyClicks(lcd(window), "2");
    ASSERT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "2"; }, 10000));
    EXPECT_TRUE(details->isEnabled());
    QTest::keyClicks(lcd(window), "00000!");  // too long while typing: dropped
    ASSERT_TRUE(QTest::qWaitFor([&] { return message(window)->text().startsWith("Too long"); }, 10000));
    EXPECT_FALSE(details->isEnabled());
}

TEST(MainWindow, TheCopyMenuOffersEveryForm) {
    MainWindow window;
    auto* button = child<QToolButton>(window, "copyButton");
    EXPECT_FALSE(button->isEnabled());  // nothing to copy yet
    EXPECT_FALSE(button->accessibleName().isEmpty());
    run(window, "0.1 + 0.2");
    EXPECT_TRUE(button->isEnabled());
    child<QAction>(window, "copy:bound")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "0.3000000000000000444089209850062616169452667236328125 ± 4.4e-17");
    child<QAction>(window, "copy:expression")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "0.1 + 0.2");
    child<QAction>(window, "copy:trusted")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "0.300000000000000");
    QTest::keyClick(lcd(window), Qt::Key_C, Qt::ControlModifier);  // nothing selected: the value
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "0.3000000000000000444089209850062616169452667236328125");
    auto* copyAs = child<QMenu>(window, "copyAsMenu");  // in the edit menu, the forms under "Copy as"
    EXPECT_TRUE(lcd(window)->editMenu()->actions().contains(copyAs->menuAction()));
    EXPECT_TRUE(copyAs->actions().contains(child<QAction>(window, "copy:value")));
}

TEST(MainWindow, WithNoTrustedDigitTheTrustedFormIsOff) {
    MainWindow window;
    run(window, "1e-17+1-1");  // 0, with a bound of 1e-17: no digit trusted
    EXPECT_TRUE(child<QAction>(window, "copy:value")->isEnabled());
    EXPECT_FALSE(child<QAction>(window, "copy:trusted")->isEnabled());
    EXPECT_EQ(child<QAction>(window, "copy:trusted")->toolTip(), "No digit is trusted");
    run(window, "1+1");
    EXPECT_TRUE(child<QAction>(window, "copy:trusted")->isEnabled());
}

TEST(MainWindow, ControlShiftCOpensTheCopyMenu) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* menu = child<QMenu>(window, "copyMenu");
    QTest::keyClick(lcd(window), Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    EXPECT_FALSE(menu->isVisible());  // nothing to copy yet
    run(window, "1+1");
    QTest::keyClick(lcd(window), Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
    EXPECT_TRUE(QTest::qWaitFor([&] { return menu->isVisible(); }, 2000));
    menu->hide();
}

TEST(MainWindow, HistoryEntriesCanBeCopied) {
    MainWindow window;
    run(window, "1/4");
    run(window, "2+2");
    auto* history = child<QListWidget>(window, "history");
    EXPECT_EQ(history->contextMenuPolicy(), Qt::CustomContextMenu);
    history->setCurrentRow(1);  // 1/4, the older one
    child<QAction>(window, "history:copyValue")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "0.25");
    child<QAction>(window, "history:copyExpression")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "1÷4");  // as the screen wrote it
    child<QAction>(window, "history:copyBound")->trigger();
    EXPECT_TRUE(QGuiApplication::clipboard()->text().startsWith("0.25 ± "));
}

TEST(MainWindow, ALongPressOnAHistoryRowOpensItsMenu) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    run(window, "1/4");
    lcd(window)->clear();  // so that a replay of the row would show
    auto* history = child<QListWidget>(window, "history");
    auto* menu = child<QMenu>(window, "historyMenu");
    const QPoint where = history->visualItemRect(history->item(0)).center();
    QTest::mousePress(history->viewport(), Qt::LeftButton, {}, where);
    EXPECT_TRUE(QTest::qWaitFor([&] { return menu->isVisible(); }, 2000));  // after half a second, without a release
    menu->hide();
    QTest::mouseRelease(history->viewport(), Qt::LeftButton, {}, where);
    EXPECT_EQ(lcd(window)->input(), "");  // the release after a long press does not replay the row
}

TEST(MainWindow, APressThatMovesIsNoLongPress) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    run(window, "1/4");
    auto* history = child<QListWidget>(window, "history");
    auto* menu = child<QMenu>(window, "historyMenu");
    const QPoint where = history->visualItemRect(history->item(0)).center();
    QTest::mousePress(history->viewport(), Qt::LeftButton, {}, where);
    QTest::mouseMove(history->viewport(), where + QPoint(0, 3 * QApplication::startDragDistance()));  // scrolling
    QTest::qWait(800);
    EXPECT_FALSE(menu->isVisible());
    QTest::mouseRelease(history->viewport(), Qt::LeftButton, {}, where);
}

TEST(MainWindow, TheHistoryMenuBelongsToTheHistoryPanel) {
    // The browser keeps an open popup above any later popup that is not its child: a row's menu must belong to the
    // panel it opens from, or it opens hidden behind it.
    MainWindow window;
    EXPECT_EQ(child<QMenu>(window, "historyMenu")->parentWidget(), child<QWidget>(window, "historyPanel"));
}

TEST(MainWindow, CopyAsIsOffWithNothingToCopy) {
    MainWindow window;
    auto* copyAs = child<QMenu>(window, "copyAsMenu");
    EXPECT_EQ(copyAs->title(), "Copy as");
    EXPECT_FALSE(copyAs->menuAction()->isEnabled());
    run(window, "1+1");
    EXPECT_TRUE(copyAs->menuAction()->isEnabled());
    run(window, "1/0");
    EXPECT_FALSE(copyAs->menuAction()->isEnabled());
}

TEST(MainWindow, CompletionsDropDownUnderTheName) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* list = child<QListWidget>(window, "completions");
    QTest::keyClicks(lcd(window), "a");
    EXPECT_FALSE(list->isVisible());  // from the second letter
    QTest::keyClicks(lcd(window), "s");
    ASSERT_TRUE(list->isVisible());
    EXPECT_EQ(list->currentItem()->text(), "asec");  // the first in alphabetical order
    EXPECT_GE(list->mapTo(&window, QPoint(0, 0)).y(),
              lcd(window)->mapTo(&window, lcd(window)->caretRectAt(lcd(window)->entry().position()).bottomLeft().toPoint()).y());
    EXPECT_TRUE(window.rect().contains(QRect(list->mapTo(&window, QPoint(0, 0)), list->size())));  // inside the window
    QTest::keyClicks(lcd(window), "x");  // nothing starts with asx
    EXPECT_FALSE(list->isVisible());
}

TEST(MainWindow, MovingTheCursorClosesTheCompletions) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* list = child<QListWidget>(window, "completions");
    QTest::keyClicks(lcd(window), "as");
    ASSERT_TRUE(list->isVisible());
    QTest::keyClick(lcd(window), Qt::Key_Left);
    EXPECT_FALSE(list->isVisible());
}

TEST(MainWindow, TabOrEnterChoosesACompletionAndEscCloses) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* list = child<QListWidget>(window, "completions");
    QTest::keyClicks(lcd(window), "asi");
    QTest::keyClick(lcd(window), Qt::Key_Down);
    QTest::keyClick(lcd(window), Qt::Key_Tab);
    EXPECT_EQ(lcd(window)->input(), "asinh(");
    EXPECT_FALSE(list->isVisible());
    lcd(window)->clear();
    QTest::keyClicks(lcd(window), "sqr");
    QTest::keyClick(lcd(window), Qt::Key_Return);  // chooses, does not evaluate
    EXPECT_EQ(lcd(window)->entry().root()[0].kind, Template::Sqrt);
    lcd(window)->clear();
    QTest::keyClicks(lcd(window), "co");
    QTest::keyClick(lcd(window), Qt::Key_Escape);  // closes the list, keeps the input
    EXPECT_FALSE(list->isVisible());
    EXPECT_EQ(lcd(window)->input(), "co");
}

TEST(MainWindow, UpGoesBackAndAClickChoosesACompletion) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* list = child<QListWidget>(window, "completions");
    QTest::keyClicks(lcd(window), "asi");
    QTest::keyClick(lcd(window), Qt::Key_Down);
    QTest::keyClick(lcd(window), Qt::Key_Up);
    EXPECT_EQ(list->currentRow(), 0);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(1)).center());
    EXPECT_EQ(lcd(window)->input(), "asinh(");
    EXPECT_FALSE(list->isVisible());
    EXPECT_TRUE(lcd(window)->hasFocus());
}

TEST(MainWindow, TheDecimalSeparatorFollowsTheLanguageUnlessChosen) {
    MainWindow window;
    EXPECT_TRUE(setting(window, "decimal:language")->isChecked());
    EXPECT_FALSE(settings::decimalComma());  // English
    setting(window, "language:es")->trigger();
    EXPECT_TRUE(settings::decimalComma());
    setting(window, "decimal:point")->trigger();
    EXPECT_FALSE(settings::decimalComma());
    setting(window, "decimal:comma")->trigger();
    setting(window, "language:en")->trigger();
    EXPECT_TRUE(settings::decimalComma());  // chosen: the language no longer matters
    setting(window, "decimal:language")->trigger();
    EXPECT_FALSE(settings::decimalComma());
}

TEST(MainWindow, TheScreenTheHistoryAndTheCopiesUseTheDecimalComma) {
    MainWindow window;
    setting(window, "decimal:comma")->trigger();
    run(window, "1.5+1");
    EXPECT_EQ(lcd(window)->outputText(), "2,5");
    auto* history = child<QListWidget>(window, "history");
    EXPECT_EQ(history->item(0)->text(), "1,5+1 = 2,5");
    child<QAction>(window, "copy:expression")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "1,5+1");
    history->setCurrentRow(0);
    child<QAction>(window, "history:copyExpression")->trigger();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), "1,5+1");
    setting(window, "decimal:language")->trigger();  // English: the point again, everywhere
    EXPECT_EQ(lcd(window)->outputText(), "2.5");
    EXPECT_EQ(history->item(0)->text(), "1.5+1 = 2.5");
}

TEST(MainWindow, ANameStillBeingTypedShowsNoError) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "2+as");
    QTest::qWait(800);  // well past the typing delay
    EXPECT_EQ(message(window)->text(), "");
    EXPECT_EQ(lcd(window)->markedText(), "");
    QTest::keyClicks(lcd(window), "x");  // nothing starts with asx: now it is an unknown name
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->text() == "Unknown name “asx”"; }, 10000))
        << message(window)->text().toStdString();
}

TEST(MainWindow, SettingsExportAndImportRoundTrip) {
    MainWindow window;
    setting(window, "theme:dark")->trigger();
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Binary128);
    const QByteArray file = window.exportSettings();
    setting(window, "theme:light")->trigger();
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Double);
    EXPECT_TRUE(window.importSettings(file).isEmpty());
    EXPECT_TRUE(setting(window, "theme:dark")->isChecked());
    EXPECT_EQ(child<TypeChooser>(window, "type")->currentType(), calculate_core::NumberType::Binary128);
    EXPECT_FALSE(window.importSettings("{}").isEmpty());  // refused as a whole: nothing changes
    EXPECT_TRUE(setting(window, "theme:dark")->isChecked());
    setting(window, "theme:system")->trigger();
}

TEST(MainWindow, EverySettingIsInTheFile) {
    MainWindow window;
    const QMap<QString, QStringList> keys = window.settingKeys();
    for (QAction* action : child<QMenu>(window, "settings")->actions()) {
        if (!action->isCheckable()) continue;
        const QString key = action->objectName().section(':', 0, 0);
        EXPECT_TRUE(keys.contains(key)) << action->objectName().toStdString();
    }
    EXPECT_TRUE(keys.contains("type"));
    EXPECT_TRUE(keys.contains("angle"));
    EXPECT_NE(child<QAction>(window, "settings:export"), nullptr);
    EXPECT_NE(child<QAction>(window, "settings:import"), nullptr);
}

TEST(MainWindow, AFreshWindowExportsNoSettings) {
    MainWindow window;
    EXPECT_TRUE(QJsonDocument::fromJson(window.exportSettings()).object().value("settings").toObject().isEmpty());
    setting(window, "decimal:comma")->trigger();
    EXPECT_EQ(QJsonDocument::fromJson(window.exportSettings()).object().value("settings").toObject().value("decimal").toString(), "comma");
    setting(window, "decimal:language")->trigger();
}

namespace {

// Characters with the Unicode Emoji property (emoji-data.txt), less the digits, # and *, which only become emoji
// with a keycap; and the selector that asks for an emoji picture.
bool isEmoji(char32_t c) {
    static const std::pair<char32_t, char32_t> ranges[] = {
        {0x00A9, 0x00A9}, {0x00AE, 0x00AE}, {0x203C, 0x203C}, {0x2049, 0x2049}, {0x2122, 0x2122}, {0x2139, 0x2139},
        {0x2194, 0x2199}, {0x21A9, 0x21AA}, {0x231A, 0x231B}, {0x2328, 0x2328}, {0x23CF, 0x23CF}, {0x23E9, 0x23F3},
        {0x23F8, 0x23FA}, {0x24C2, 0x24C2}, {0x25AA, 0x25AB}, {0x25B6, 0x25B6}, {0x25C0, 0x25C0}, {0x25FB, 0x25FE},
        {0x2600, 0x2604}, {0x260E, 0x260E}, {0x2611, 0x2611}, {0x2614, 0x2615}, {0x2618, 0x2618}, {0x261D, 0x261D},
        {0x2620, 0x2620}, {0x2622, 0x2623}, {0x2626, 0x2626}, {0x262A, 0x262A}, {0x262E, 0x262F}, {0x2638, 0x263A},
        {0x2640, 0x2640}, {0x2642, 0x2642}, {0x2648, 0x2653}, {0x265F, 0x2660}, {0x2663, 0x2663}, {0x2665, 0x2666},
        {0x2668, 0x2668}, {0x267B, 0x267B}, {0x267E, 0x267F}, {0x2692, 0x2697}, {0x2699, 0x2699}, {0x269B, 0x269C},
        {0x26A0, 0x26A1}, {0x26A7, 0x26A7}, {0x26AA, 0x26AB}, {0x26B0, 0x26B1}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5},
        {0x26C8, 0x26C8}, {0x26CE, 0x26CF}, {0x26D1, 0x26D1}, {0x26D3, 0x26D4}, {0x26E9, 0x26EA}, {0x26F0, 0x26F5},
        {0x26F7, 0x26FA}, {0x26FD, 0x26FD}, {0x2702, 0x2702}, {0x2705, 0x2705}, {0x2708, 0x270D}, {0x270F, 0x270F},
        {0x2712, 0x2712}, {0x2714, 0x2714}, {0x2716, 0x2716}, {0x271D, 0x271D}, {0x2721, 0x2721}, {0x2728, 0x2728},
        {0x2733, 0x2734}, {0x2744, 0x2744}, {0x2747, 0x2747}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755},
        {0x2757, 0x2757}, {0x2763, 0x2764}, {0x2795, 0x2797}, {0x27A1, 0x27A1}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF},
        {0x2934, 0x2935}, {0x2B05, 0x2B07}, {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x3030, 0x3030},
        {0x303D, 0x303D}, {0x3297, 0x3297}, {0x3299, 0x3299}, {0x1F000, 0x1FAFF}, {0xFE0F, 0xFE0F}};
    return std::any_of(std::begin(ranges), std::end(ranges), [c](const auto& r) { return c >= r.first && c <= r.second; });
}

// Every text the window shows: legends, tooltips, labels, menu entries and placeholders.
QStringList shownTexts(MainWindow& window) {
    QStringList texts;
    for (QAbstractButton* b : window.findChildren<QAbstractButton*>()) texts << b->text() << b->toolTip();
    for (QLabel* l : window.findChildren<QLabel*>()) texts << l->text();
    for (QAction* a : window.findChildren<QAction*>()) texts << a->text();
    for (QLineEdit* e : window.findChildren<QLineEdit*>()) texts << e->placeholderText();
    return texts;
}

// The colour of the icon's solid pixels.
QColor ink(const QIcon& icon) {
    const QImage image = icon.pixmap(QSize(32, 32)).toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) == 255) return QColor(image.pixel(x, y));
    return {};
}

}  // namespace

TEST(MainWindow, NoEmojiAnywhere) {
    MainWindow window;
    for (const bool spanish : {false, true}) {
        setting(window, spanish ? "language:es" : "language:en")->trigger();
        QCoreApplication::processEvents();
        for (const QString& text : shownTexts(window))
            for (char32_t c : text.toUcs4()) EXPECT_FALSE(isEmoji(c)) << text.toStdString();
    }
    setting(window, "language:system")->trigger();
    for (const char* name : {"settingsButton", "statisticsKeysToggle"}) {
        auto* button = child<QToolButton>(window, name);
        EXPECT_TRUE(button->text().isEmpty()) << name;  // a drawn icon instead
        EXPECT_FALSE(button->icon().isNull()) << name;
        EXPECT_FALSE(button->toolTip().isEmpty()) << name;
    }
}

TEST(MainWindow, IconsFollowTheTheme) {
    MainWindow window;
    auto* gear = child<QToolButton>(window, "settingsButton");
    for (const char* theme : {"theme:dark", "theme:light"}) {
        setting(window, theme)->trigger();
        QCoreApplication::processEvents();
        EXPECT_EQ(ink(gear->icon()).rgb(), window.palette().color(QPalette::ButtonText).rgb()) << theme;
    }
    setting(window, "theme:system")->trigger();
}

TEST(MainWindow, TheLeftColumnShowsCommonMemoryAndTheSections) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const auto top = [&](const QString& name) {
        QWidget* w = window.findChild<QWidget*>(name);
        EXPECT_NE(w, nullptr) << name.toStdString();
        return w ? w->mapTo(&window, QPoint(0, 0)).y() : 0;
    };
    auto* common = child<QWidget>(window, "common");
    for (const QString& id : defaultCommon())
        EXPECT_NE(common->findChild<QPushButton*>("common:" + id), nullptr) << id.toStdString();
    EXPECT_LT(top("common"), top("memoryKeys"));  // Common first, then Memory and editing
    for (const Key& key : memoryKeys())
        EXPECT_NE(child<QWidget>(window, "memoryKeys")->findChild<QPushButton*>("direct:" + key.id), nullptr);
    int last = top("memoryKeys");
    for (const KeySection& s : keySections()) {
        const QString header = "section:" + s.id;
        EXPECT_EQ(child<QToolButton>(window, header.toUtf8().constData())->text(), translated(s.title));
        EXPECT_GT(top(header), last) << s.id.toStdString();  // in the table's order, under Memory and editing
        last = top(header);
        auto* keys = child<QWidget>(window, ("sectionKeys:" + s.id).toUtf8().constData());
        for (const Key& key : s.keys) EXPECT_NE(keys->findChild<QPushButton*>("direct:" + key.id), nullptr) << key.id.toStdString();
    }
    EXPECT_EQ(child<QPushButton>(window, "common:asin")->size(), child<QPushButton>(window, "key:sin")->size());
}

TEST(MainWindow, ACommonKeyDoesWhatItsSectionKeyDoes) {
    MainWindow window;
    QTest::mouseClick(child<QPushButton>(window, "common:asin"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "common:pi"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "asin(π");
    EXPECT_EQ(child<QPushButton>(window, "common:asin")->text(), child<QPushButton>(window, "direct:asin")->text());
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_FALSE(child<QPushButton>(window, "common:pi")->isEnabled());  // Exact greys it out like its twin
    EXPECT_FALSE(child<QPushButton>(window, "common:pi")->toolTip().isEmpty());
}

TEST(MainWindow, SectionsOpenInPlaceAndMoveNothingAbove) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const auto rect = [&](const QString& name) {
        QWidget* w = window.findChild<QWidget*>(name);
        EXPECT_NE(w, nullptr) << name.toStdString();
        return w ? QRect(w->mapTo(&window, QPoint(0, 0)), w->size()) : QRect();
    };
    const QRect keypad = rect("keypad"), screen = rect("lcd"), common = rect("common"), memory = rect("memoryKeys");
    for (const KeySection& s : keySections()) {
        auto* header = child<QToolButton>(window, ("section:" + s.id).toUtf8().constData());
        auto* preview = child<QLabel>(window, ("preview:" + s.id).toUtf8().constData());
        EXPECT_TRUE(header->isCheckable());
        EXPECT_FALSE(header->isChecked()) << s.id.toStdString();  // closed at start
        EXPECT_EQ(header->arrowType(), Qt::RightArrow);
        EXPECT_FALSE(child<QWidget>(window, ("sectionKeys:" + s.id).toUtf8().constData())->isVisible());
        EXPECT_TRUE(preview->isVisible());
        EXPECT_TRUE(preview->text().startsWith(translated(s.keys.first().face.label))) << s.id.toStdString();
    }
    QTest::mouseClick(child<QToolButton>(window, "section:hyperbolic"), Qt::LeftButton);
    QTest::mouseClick(child<QToolButton>(window, "section:constants"), Qt::LeftButton);
    QTest::qWait(50);
    EXPECT_TRUE(child<QWidget>(window, "sectionKeys:hyperbolic")->isVisible());  // several open at once
    EXPECT_TRUE(child<QWidget>(window, "sectionKeys:constants")->isVisible());
    EXPECT_FALSE(child<QWidget>(window, "sectionKeys:numbers")->isVisible());
    EXPECT_FALSE(child<QLabel>(window, "preview:hyperbolic")->isVisible());
    EXPECT_EQ(child<QToolButton>(window, "section:hyperbolic")->arrowType(), Qt::DownArrow);
    EXPECT_EQ(rect("keypad"), keypad);  // opening moves neither the pad, the screen nor what is always shown
    EXPECT_EQ(rect("lcd"), screen);
    EXPECT_EQ(rect("common"), common);
    EXPECT_EQ(rect("memoryKeys"), memory);
    QTest::mouseClick(child<QToolButton>(window, "section:hyperbolic"), Qt::LeftButton);
    QTest::qWait(50);
    EXPECT_FALSE(child<QWidget>(window, "sectionKeys:hyperbolic")->isVisible());
    EXPECT_TRUE(child<QLabel>(window, "preview:hyperbolic")->isVisible());
}

TEST(MainWindow, ALongColumnScrollsOnItsOwn) {
    MainWindow window;
    window.resize(1200, 560);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    QWidget* pad = child<QWidget>(window, "keypad");
    const QRect keypad(pad->mapTo(&window, QPoint(0, 0)), pad->size());
    for (const KeySection& s : keySections()) openSection(window, s.id);
    QTest::qWait(50);
    EXPECT_TRUE(child<QScrollArea>(window, "directScroll")->verticalScrollBar()->isVisible());
    EXPECT_FALSE(child<QScrollArea>(window, "keys")->verticalScrollBar()->isVisible());  // the pad does not scroll away
    EXPECT_EQ(QRect(pad->mapTo(&window, QPoint(0, 0)), pad->size()), keypad);  // nor moves when the bar appears
}

TEST(MainWindow, TheStatisticsSectionTypesAStatistic) {
    MainWindow window;
    openSection(window, "statistics");
    for (const char* name : {"direct:mean", "key:2", "key:close"}) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "mean(2)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "2");
}

TEST(MainWindow, EveryKeyLabelFitsItsKey) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const int smallest = qRound(0.6 * QFontInfo(window.font()).pixelSize());
    for (QPushButton* key : window.findChildren<QPushButton*>(QRegularExpression("^(key|direct|common):"))) {
        const bool fits = QFontMetrics(key->font()).horizontalAdvance(key->text()) <= key->width() - 10;
        EXPECT_TRUE(fits || QFontInfo(key->font()).pixelSize() <= smallest) << key->objectName().toStdString();
    }
}

TEST(MainWindow, MemoryStoreReplacesTheMemoryAndTheScreenShowsM) {
    MainWindow window;
    QTest::mouseClick(child<QPushButton>(window, "direct:memoryStore"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return message(window)->text() == "The memory needs a previous result"; }, 10000));
    run(window, "5");
    QTest::mouseClick(child<QPushButton>(window, "key:memoryAdd"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory() == "5"; }, 10000));
    run(window, "7");
    QTest::mouseClick(child<QPushButton>(window, "direct:memoryStore"), Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->memory() == "7"; }, 10000)) << lcd(window)->memory().toStdString();
    EXPECT_EQ(lcd(window)->statusText(), "M");  // the indicator, as on the calculator
    EXPECT_EQ(lcd(window)->toolTip(), "M = 7");
    EXPECT_EQ(child<QPushButton>(window, "direct:memoryStore")->text(), "MS");
}

TEST(MainWindow, OnAPhoneThePadSitsAtTheBottomWithCommonAboveIt) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));  // as on a phone's screen
    QTest::qWait(50);
    const auto rect = [&](const char* name) {
        QWidget* w = child<QWidget>(window, name);
        return QRect(w->mapTo(&window, QPoint(0, 0)), w->size());
    };
    EXPECT_FALSE(child<QWidget>(window, "rail")->isVisible());
    EXPECT_GE(rect("lcd").width(), window.width() - 40);  // the screen at the full width…
    for (const char* name : {"detailsButton", "copyButton", "editButton", "historyToggle"}) {
        auto* button = child<QToolButton>(window, name);
        EXPECT_TRUE(button->isVisible()) << name;  // … shows its whole bar
        EXPECT_GE(button->width(), button->sizeHint().width()) << name;
        EXPECT_TRUE(rect("lcd").contains(rect(name))) << name;
    }
    EXPECT_LT(rect("lcd").bottom(), rect("angle").top());  // ☰, angle, type, = and the gear in one row under it
    for (const char* name : {"type", "equals"}) EXPECT_EQ(rect(name).top(), rect("angle").top()) << name;
    for (const char* name : {"panelToggle", "settingsButton"}) {
        const int middle = rect(name).center().y();
        EXPECT_TRUE(middle >= rect("angle").top() && middle <= rect("angle").bottom()) << name;
    }
    EXPECT_LT(rect("panelToggle").right(), rect("angle").left());
    EXPECT_GT(rect("settingsButton").left(), rect("equals").right());
    EXPECT_LT(rect("angle").bottom(), rect("common").top());
    EXPECT_LT(rect("common").bottom(), rect("keypad").top());          // Common above the pad
    EXPECT_GT(rect("keypad").center().y(), window.height() / 2);      // the pad in the lower half, in thumb reach
    EXPECT_LE(rect("keypad").bottom(), window.height());
    auto* keys = child<QScrollArea>(window, "keys");
    EXPECT_FALSE(keys->horizontalScrollBar()->isVisible());           // nothing scrolls
    EXPECT_FALSE(keys->verticalScrollBar()->isVisible());
    for (const char* name : {"key:7", "key:sin", "common:asin"}) EXPECT_GE(child<QWidget>(window, name)->height(), touchTarget) << name;
    auto* toggle = child<QPushButton>(window, "drawerToggle");
    EXPECT_TRUE(toggle->isVisible());
    EXPECT_EQ(toggle->size(), child<QPushButton>(window, "common:asin")->size());
    EXPECT_FALSE(child<QPushButton>(window, "common:abs")->isVisible());  // its place holds More; abs is in Numbers
    EXPECT_FALSE(child<QWidget>(window, "drawer")->isVisible());
    window.layOutKeys(QSize(1920, 1200));  // and back, side by side
    QTest::qWait(50);
    EXPECT_TRUE(child<QWidget>(window, "rail")->isVisible());
    EXPECT_FALSE(toggle->isVisible());
    EXPECT_TRUE(child<QPushButton>(window, "common:abs")->isVisible());
    EXPECT_LT(rect("directKeys").right(), rect("keypad").left());
    EXPECT_LT(rect("lcd").right(), rect("angle").left());
    EXPECT_LT(rect("common").bottom(), rect("memoryKeys").top());  // Common back at the top of the column
}

TEST(MainWindow, OnAPhoneMoreOpensADrawerOverThePad) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    QTest::qWait(50);
    const auto rect = [&](const char* name) {
        QWidget* w = child<QWidget>(window, name);
        return QRect(w->mapTo(&window, QPoint(0, 0)), w->size());
    };
    auto* toggle = child<QPushButton>(window, "drawerToggle");
    auto* drawer = child<QWidget>(window, "drawer");
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTest::qWait(50);
    ASSERT_TRUE(drawer->isVisible());
    EXPECT_TRUE(toggle->isChecked());
    EXPECT_LE(rect("drawer").top(), rect("keypad").top());  // over the pad…
    EXPECT_GE(rect("drawer").bottom(), rect("keypad").bottom());
    EXPECT_GT(rect("drawer").top(), rect("common").bottom());  // … under the screen and Common, which stay in view
    EXPECT_TRUE(drawer->isAncestorOf(child<QWidget>(window, "memoryKeys")));
    EXPECT_TRUE(drawer->isAncestorOf(child<QWidget>(window, "section:numbers")));
    EXPECT_FALSE(drawer->isAncestorOf(child<QWidget>(window, "common")));
    openSection(window, "hyperbolic");
    EXPECT_TRUE(drawer->isVisible());  // opening a section keeps the drawer open
    QTest::mouseClick(child<QPushButton>(window, "direct:sinh"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sinh(");
    EXPECT_FALSE(drawer->isVisible());  // a key types and brings back the pad
    EXPECT_FALSE(toggle->isChecked());
    QTest::mouseClick(toggle, Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "drawerClose"), Qt::LeftButton);
    EXPECT_FALSE(drawer->isVisible());
    EXPECT_FALSE(toggle->isChecked());
}

TEST(MainWindow, OnAPhoneTheMenuButtonListsTheModes) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    QTest::mouseClick(child<QToolButton>(window, "panelToggle"), Qt::LeftButton);
    auto* menu = child<QMenu>(window, "modesMenu");
    ASSERT_TRUE(menu->isVisible());
    EXPECT_TRUE(window.geometry().contains(menu->geometry()));
    EXPECT_EQ(child<QAction>(window, "mode:1")->text(), child<QListWidget>(window, "modes")->item(1)->text());
    child<QAction>(window, "mode:1")->trigger();
    EXPECT_EQ(child<QListWidget>(window, "modes")->currentRow(), 1);
    EXPECT_EQ(child<QStackedWidget>(window, "pages")->currentIndex(), 1);
}

TEST(MainWindow, LetterKeysTypeNamesThatBecomePieces) {
    MainWindow window;
    openSection(window, "letters");
    for (const char* name : {"direct:letterS", "direct:letterI", "direct:letterN", "key:open"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    ASSERT_EQ(lcd(window)->entry().root().size(), 1u);  // s, i, n and ( became the one piece sin(
    EXPECT_EQ(lcd(window)->input(), "sin(");
    QTest::mouseClick(child<QPushButton>(window, "direct:shift"), Qt::LeftButton);
    EXPECT_EQ(child<QPushButton>(window, "direct:letterA")->text(), "A");  // the legends follow the shift
    QTest::mouseClick(child<QPushButton>(window, "direct:letterA"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(A");
    EXPECT_EQ(child<QPushButton>(window, "direct:letterA")->text(), "a");  // one capital, then small again
    QTest::mouseClick(child<QPushButton>(window, "direct:space"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sin(A ");
    QTest::mouseClick(child<QPushButton>(window, "key:clear"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "key:open"), Qt::LeftButton);  // no name before it: a plain (
    EXPECT_EQ(lcd(window)->input(), "(");
}

TEST(MainWindow, UndoAndRedoWithKeys) {
    MainWindow window;
    for (const char* name : {"key:1", "key:plus", "key:2"}) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "direct:undo"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "1+");
    QTest::mouseClick(child<QPushButton>(window, "direct:redo"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "1+2");
}

TEST(MainWindow, TheSearchBoxFindsAKeyAndTypesIt) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* box = child<QLineEdit>(window, "search");
    auto* list = child<QListWidget>(window, "searchList");
    const auto entries = [&] {  // the entries shown, without the headings
        QList<QListWidgetItem*> rows;
        for (int i = 0; i < list->count(); ++i)
            if (!list->item(i)->isHidden() && list->item(i)->data(Qt::UserRole).isValid()) rows << list->item(i);
        return rows;
    };
    EXPECT_FALSE(list->isVisible());
    EXPECT_FALSE(box->placeholderText().isEmpty());
    auto* icon = box->findChild<QAction*>("searchIcon");
    ASSERT_NE(icon, nullptr);
    EXPECT_FALSE(icon->icon().isNull());  // a drawn magnifier, no emoji
    EXPECT_LT(box->mapTo(&window, QPoint(0, 0)).y(), child<QWidget>(window, "common")->mapTo(&window, QPoint(0, 0)).y());
    QTest::mouseClick(box, Qt::LeftButton);
    ASSERT_TRUE(list->isVisible());  // an empty box lists everything: finding needs no keyboard
    EXPECT_EQ(entries().size(), searchEntries().size());
    EXPECT_TRUE(QRect(QPoint(0, 0), window.size()).contains(QRect(list->mapTo(&window, QPoint(0, 0)), list->size())));
    QTest::keyClicks(box, "acosh");
    ASSERT_EQ(entries().size(), 1);
    EXPECT_EQ(entries().first()->text(), "acosh");
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(entries().first()).center());
    EXPECT_EQ(lcd(window)->input(), "acosh(");
    EXPECT_FALSE(list->isVisible());
    EXPECT_TRUE(box->text().isEmpty());
    EXPECT_EQ(window.focusWidget(), lcd(window));  // back to the screen
    QTest::mouseClick(box, Qt::LeftButton);
    QTest::keyClicks(box, "x");
    QTest::keyClick(box, Qt::Key_Escape);
    EXPECT_FALSE(list->isVisible());
    EXPECT_TRUE(box->text().isEmpty());
}

TEST(MainWindow, KeysHaveNamesForScreenReaders) {
    MainWindow window;
    for (QPushButton* key : window.findChildren<QPushButton*>(QRegularExpression("^(key|direct|common):")))
        EXPECT_FALSE(key->accessibleName().isEmpty()) << key->objectName().toStdString();
    EXPECT_EQ(child<QPushButton>(window, "key:reciprocal")->accessibleName(), "reciprocal");  // words, not symbols
    EXPECT_EQ(child<QPushButton>(window, "direct:undo")->accessibleName(), "undo");
    EXPECT_EQ(child<QPushButton>(window, "direct:space")->accessibleName(), "space");
    EXPECT_EQ(child<QPushButton>(window, "common:cbrt")->accessibleName(), "Cube root");  // a function's title
    EXPECT_EQ(child<QToolButton>(window, "section:statistics")->accessibleName(), "Statistics");
    EXPECT_EQ(child<QLineEdit>(window, "search")->accessibleName(), "Search");
    EXPECT_EQ(child<QPushButton>(window, "drawerToggle")->accessibleName(), "More");
}

TEST(MainWindow, TheColumnIsAsWideAsItsKeys) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    QWidget* column = child<QWidget>(window, "directKeys");
    const QMargins margins = column->layout()->contentsMargins();
    const int keys = child<QWidget>(window, "common")->sizeHint().width() + margins.left() + margins.right();
    const int bar = window.style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    EXPECT_LE(child<QScrollArea>(window, "directScroll")->width(), keys + bar);  // a long preview is cut, not widening it
}

TEST(MainWindow, TheColumnAndTheSearchListScrollWithAFinger) {
    MainWindow window;
    // On a phone the column is the drawer's content: a finger must scroll it, as it scrolls the search list.
    EXPECT_TRUE(QScroller::hasScroller(child<QScrollArea>(window, "directScroll")->viewport()));
    EXPECT_TRUE(QScroller::hasScroller(child<QListWidget>(window, "searchList")->viewport()));
}

TEST(MainWindow, CommonCanBeChangedAndReset) {
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    EXPECT_EQ(window.common(), defaultCommon());
    auto* edit = child<QToolButton>(window, "editCommon");
    auto* reset = child<QToolButton>(window, "resetCommon");
    EXPECT_FALSE(reset->isVisible());
    QTest::mouseClick(edit, Qt::LeftButton);
    EXPECT_TRUE(edit->isChecked());
    EXPECT_TRUE(reset->isVisible());
    EXPECT_FALSE(message(window)->text().isEmpty());  // says what a click does now
    openSection(window, "hyperbolic");
    QTest::mouseClick(child<QPushButton>(window, "direct:sinh"), Qt::LeftButton);
    EXPECT_EQ(message(window)->text(), "Common holds at most 12 keys");  // full
    QTest::mouseClick(child<QPushButton>(window, "common:abs"), Qt::LeftButton);
    QStringList expected = defaultCommon();
    expected.removeOne("abs");
    EXPECT_EQ(window.common(), expected);
    QTest::mouseClick(child<QPushButton>(window, "direct:sinh"), Qt::LeftButton);
    expected << "sinh";
    EXPECT_EQ(window.common(), expected);
    EXPECT_EQ(lcd(window)->input(), "");  // editing types nothing
    QCoreApplication::processEvents();
    auto* sinh = child<QPushButton>(window, "common:sinh");
    EXPECT_TRUE(sinh->isVisible());
    EXPECT_EQ(sinh->size(), child<QPushButton>(window, "key:sin")->size());
    EXPECT_EQ(sinh->text(), child<QPushButton>(window, "direct:sinh")->text());
    QTest::mouseClick(edit, Qt::LeftButton);  // done: keys type again
    EXPECT_FALSE(reset->isVisible());
    EXPECT_EQ(message(window)->text(), "");
    QTest::mouseClick(sinh, Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "sinh(");
    QTest::mouseClick(edit, Qt::LeftButton);
    QTest::mouseClick(reset, Qt::LeftButton);
    EXPECT_EQ(window.common(), defaultCommon());
    QTest::mouseClick(edit, Qt::LeftButton);
}

TEST(MainWindow, AChangedCommonTravelsInTheSettingsFile) {
    MainWindow window;
    EXPECT_TRUE(window.settingKeys().value("common").contains("sinh"));
    QTest::mouseClick(child<QToolButton>(window, "editCommon"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "common:abs"), Qt::LeftButton);
    openSection(window, "hyperbolic");
    QTest::mouseClick(child<QPushButton>(window, "direct:sinh"), Qt::LeftButton);
    QTest::mouseClick(child<QToolButton>(window, "editCommon"), Qt::LeftButton);
    QStringList expected = defaultCommon();
    expected.removeOne("abs");
    expected << "sinh";
    const QByteArray file = window.exportSettings();
    EXPECT_EQ(QJsonDocument::fromJson(file).object().value("settings").toObject().value("common").toString(), expected.join(' '));
    MainWindow other;
    EXPECT_TRUE(other.importSettings(file).isEmpty());
    EXPECT_EQ(other.common(), expected);
    EXPECT_NE(other.findChild<QPushButton*>("common:sinh"), nullptr);
    EXPECT_FALSE(other.importSettings(R"({"version":1,"settings":{"common":"sinh sinh"}})").isEmpty());
    EXPECT_EQ(other.common(), expected);  // refused: unchanged
}

TEST(MainWindow, OnAPhoneCommonIsEditedFromTheDrawer) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    QTest::mouseClick(child<QPushButton>(window, "drawerToggle"), Qt::LeftButton);
    auto* drawer = child<QWidget>(window, "drawer");
    EXPECT_TRUE(drawer->isAncestorOf(child<QWidget>(window, "editCommon")));
    QTest::mouseClick(child<QToolButton>(window, "editCommon"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(window, "common:asin"), Qt::LeftButton);  // the strip above the drawer
    openSection(window, "numbers");
    QTest::mouseClick(child<QPushButton>(window, "direct:gcd"), Qt::LeftButton);
    EXPECT_EQ(window.common().last(), "gcd");
    EXPECT_FALSE(window.common().contains("asin"));
    EXPECT_TRUE(drawer->isVisible());  // editing keeps the drawer open
    QTest::mouseClick(child<QToolButton>(window, "editCommon"), Qt::LeftButton);
    window.layOutKeys(QSize(1920, 1200));
    EXPECT_FALSE(drawer->isAncestorOf(child<QWidget>(window, "editCommon")));  // back on Common's title row
}

TEST(MainWindow, ARightClickOffersTheKeysOtherFaces) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    auto* sin = child<KeyButton>(window, "key:sin");
    ASSERT_TRUE(sin->hasMore());
    QTest::mouseClick(sin, Qt::RightButton);
    auto* popup = child<QFrame>(window, "moreKeys");
    ASSERT_TRUE(popup->isVisible());
    EXPECT_TRUE(window.geometry().contains(popup->geometry()));
    QTest::mouseClick(child<QPushButton>(window, "more:sin:0"), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "asin(");
    EXPECT_FALSE(popup->isVisible());
}

TEST(MainWindow, ThePercentagesModeAnswersWithTheirBounds) {
    MainWindow window;
    auto* modes = child<QListWidget>(window, "modes");
    ASSERT_EQ(modes->count(), 3);
    modes->setCurrentRow(2);
    child<QLineEdit>(window, "percentFirst")->setText("80");
    child<QLineEdit>(window, "percentSecond")->setText("10");
    QLabel* plus = child<QLabel>(window, "percent:plus");
    EXPECT_TRUE(QTest::qWaitFor([&] { return plus->text() == "88"; }, 10000)) << plus->text().toStdString();
    EXPECT_EQ(child<QLabel>(window, "percentBound:plus")->text(), "± 0");  // every answer shows its bound
    child<QLineEdit>(window, "percentSecond")->setText("0.1");
    QLabel* of = child<QLabel>(window, "percentBound:of");
    EXPECT_TRUE(QTest::qWaitFor([&] { return of->text().startsWith("± ") && of->text() != "± 0"; }, 10000))
        << of->text().toStdString();  // 0.1 is not exact in binary: the bound says so
    child<QLineEdit>(window, "percentSecond")->setText("abc");  // the statistics box's filter keeps it numeric
    EXPECT_EQ(child<QLineEdit>(window, "percentSecond")->text(), "");
}

TEST(MainWindow, ThePercentagesKeyStartsFromTheLastResult) {
    MainWindow window;
    run(window, "40*2");
    openSection(window, "numbers");
    QTest::mouseClick(child<QPushButton>(window, "direct:percentages"), Qt::LeftButton);
    EXPECT_EQ(child<QListWidget>(window, "modes")->currentRow(), 2);
    EXPECT_EQ(child<QLineEdit>(window, "percentFirst")->text(), "80");
}

TEST(MainWindow, ThePercentagesKeyKeepsALargeResultWhole) {
    MainWindow window;
    run(window, "10^30");
    openSection(window, "numbers");
    QTest::mouseClick(child<QPushButton>(window, "direct:percentages"), Qt::LeftButton);
    EXPECT_EQ(child<QLineEdit>(window, "percentFirst")->text(), "1.000000000000000019884624838656e30");  // the exponent survives the filter
}

TEST(MainWindow, OnAPhoneThePercentagesFitTheWidth) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    child<QListWidget>(window, "modes")->setCurrentRow(2);
    child<QLineEdit>(window, "percentFirst")->setText("80");
    child<QLineEdit>(window, "percentSecond")->setText("0.1");
    QLabel* of = child<QLabel>(window, "percent:of");
    ASSERT_TRUE(QTest::qWaitFor([&] { return of->text().size() > 40; }, 10000));  // a long answer, noise and all
    QTest::qWait(50);
    auto* page = child<QScrollArea>(window, "percentagesScroll");
    EXPECT_FALSE(page->horizontalScrollBar()->isVisible());  // nothing scrolls sideways…
    QLabel* bound = child<QLabel>(window, "percentBound:of");
    EXPECT_LE(bound->mapTo(&window, QPoint(bound->width(), 0)).x(), window.width());  // …and every bound is in view
    EXPECT_EQ(of->toolTip(), of->text());  // the whole answer, where it is cut short
}

TEST(MainWindow, ThePercentagesKeypadMovesToTheSecondValue) {
    MainWindow window;  // not shown: no box has the focus, as in a browser that hasn't given the page the keyboard
    child<QListWidget>(window, "modes")->setCurrentRow(2);
    for (const char* key : {"8", "next", "1", "point", "5"})
        QTest::mouseClick(child<QPushButton>(window, QStringLiteral("percentKey:%1").arg(key).toUtf8().constData()), Qt::LeftButton);
    EXPECT_EQ(child<QLineEdit>(window, "percentFirst")->text(), "8");
    EXPECT_EQ(child<QLineEdit>(window, "percentSecond")->text(), settings::decimalComma() ? "1,5" : "1.5");
    QTest::mouseClick(child<QPushButton>(window, "percentKey:next"), Qt::LeftButton);  // and back
    QTest::mouseClick(child<QPushButton>(window, "percentKey:backspace"), Qt::LeftButton);
    EXPECT_EQ(child<QLineEdit>(window, "percentFirst")->text(), "");
}

TEST(MainWindow, TheKeysShowTheSeparatorsInUse) {
    MainWindow window;
    setting(window, "decimal:comma")->trigger();
    EXPECT_EQ(child<QPushButton>(window, "key:point")->text(), ",");
    EXPECT_EQ(child<QPushButton>(window, "direct:comma")->text(), ";");
    EXPECT_EQ(child<QPushButton>(window, "percentKey:point")->text(), ",");
    setting(window, "decimal:language")->trigger();  // English: the point again
    EXPECT_EQ(child<QPushButton>(window, "key:point")->text(), ".");
    EXPECT_EQ(child<QPushButton>(window, "direct:comma")->text(), ",");
    EXPECT_EQ(child<QPushButton>(window, "percentKey:point")->text(), ".");
}

TEST(MainWindow, RemainderKeysTypeTheirFunctions) {
    MainWindow window;
    openSection(window, "numbers");
    for (const char* name : {"direct:floormod", "key:negative", "key:7", "direct:comma", "key:3", "key:close"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "floormod(-7, 3)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "2");
}

TEST(MainWindow, TheConventionsAreSettings) {
    MainWindow window;
    for (const char* name : {"log:10", "log:e", "mod:truncated", "mod:floored", "percent:divide", "percent:ofvalue"})
        EXPECT_NE(setting(window, name), nullptr) << name;
    EXPECT_TRUE(setting(window, "log:10")->isChecked());
    setting(window, "log:e")->trigger();
    run(window, "log(1)");
    EXPECT_EQ(lcd(window)->outputText(), "0");
    run(window, "log(100)");
    EXPECT_TRUE(lcd(window)->outputText().startsWith("4.60517")) << lcd(window)->outputText().toStdString();
    EXPECT_TRUE(window.exportSettings().contains("\"log\": \"e\""));  // Plan 5's file gets it for free
    setting(window, "log:10")->trigger();
}

TEST(MainWindow, TheLogKeySaysWhichLogarithm) {
    MainWindow window;
    EXPECT_TRUE(child<QPushButton>(window, "direct:log")->toolTip().endsWith("\nlogarithm (base 10)"));
    setting(window, "log:e")->trigger();
    EXPECT_TRUE(child<QPushButton>(window, "direct:log")->toolTip().endsWith("\nlogarithm (natural)"));
    setting(window, "log:10")->trigger();
}

TEST(MainWindow, TheConventionsSitApartInTheirOwnMenu) {
    MainWindow window;
    const QList<QAction*> top = child<QMenu>(window, "settings")->actions();
    const int index = top.indexOf(child<QMenu>(window, "conventions")->menuAction());
    ASSERT_GT(index, 0);
    EXPECT_TRUE(top[index - 1]->isSeparator());  // not read as one more decimal separator
    int separators = 0;
    for (QAction* action : child<QMenu>(window, "conventions")->actions()) separators += action->isSeparator();
    EXPECT_EQ(separators, 2);  // log | mod | %
}

TEST(MainWindow, TheHistoryKeepsComments) {
    MainWindow window;
    run(window, "1+1 # two");
    auto* list = child<QListWidget>(window, "history");
    EXPECT_EQ(list->item(0)->text(), "1+1 = 2   # two");
    forget(window);
    lcd(window)->clear();
    lcd(window)->setInput("# a note");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    ASSERT_TRUE(QTest::qWaitFor([&] { return list->count() == 2; }, 10000));
    EXPECT_EQ(list->item(0)->text(), "# a note");
    EXPECT_EQ(lcd(window)->outputText(), "");
    EXPECT_FALSE(child<QToolButton>(window, "detailsButton")->isEnabled());
}

TEST(MainWindow, ACommentCanBeEnteredWithKeysAlone) {
    MainWindow window;
    for (const char* name : {"key:1", "key:plus", "key:1"}) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    openSection(window, "letters");
    QTest::mouseClick(child<QPushButton>(window, "direct:comment"), Qt::LeftButton);
    for (const char* name : {"direct:space", "direct:letterT", "direct:letterW", "direct:letterO"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "1+1# two");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(child<QListWidget>(window, "history")->item(0)->text(), "1+1 = 2   # two");
}

TEST(MainWindow, AConversionTakesTheScreenAndTheCardKeepsTheValue) {
    MainWindow window;
    run(window, "0.1 to fraction");
    EXPECT_EQ(lcd(window)->outputText(), "3602879701896397/36028797018963968");
    EXPECT_EQ(detail(window, "value"), "0.1000000000000000|055511151231257827021181583404541015625");
    EXPECT_EQ(detail(window, "conversion"), "fraction");
    EXPECT_EQ(child<QListWidget>(window, "history")->item(0)->text(), "0.1 to fraction = 3602879701896397/36028797018…");
}

TEST(MainWindow, ArrowKeysConvertTheResult) {
    MainWindow window;
    for (const char* name : {"key:fraction", "key:1", "key:down", "key:4", "key:right"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    openSection(window, "showAs");
    QTest::mouseClick(child<QPushButton>(window, "direct:to:fraction"), Qt::LeftButton);
    EXPECT_TRUE(lcd(window)->input().endsWith("→fraction")) << lcd(window)->input().toStdString();
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1/4");
}

TEST(MainWindow, SumKeysBuildASum) {
    MainWindow window;
    openSection(window, "powers");
    for (const char* name : {"direct:sum", "key:1", "key:right", "key:4", "key:right", "direct:variable", "key:square"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "Σ(x^(2), 1, 4)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "30");
}

TEST(MainWindow, ANoteShowsDimmedUnderTheScreen) {
    MainWindow window;
    run(window, "sum(x; 5; 1)");
    EXPECT_EQ(lcd(window)->outputText(), "0");
    EXPECT_EQ(message(window)->text(), "Σ(x, 5, 1) has no terms");  // typed sums read back as Σ templates (1.27)
    EXPECT_TRUE(message(window)->property("dimmed").toBool());
    run(window, "1+1");
    EXPECT_EQ(message(window)->text(), "");
}

TEST(MainWindow, StoreKeepsAnExpressionInAVariable) {
    MainWindow window;
    const auto click = [&](const char* name) { QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton); };
    openSection(window, "variables");
    lcd(window)->setInput("0.1+0.2");
    forget(window);
    click("direct:store");
    EXPECT_EQ(lcd(window)->statusText(), "STO");
    click("direct:varA");  // with STO armed, the letter stores instead of typing
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->statusText(), "");
    EXPECT_EQ(child<QListWidget>(window, "history")->item(0)->data(Qt::UserRole).toString(), "A := 0.1+0.2");
    lcd(window)->clear();
    click("direct:varA");
    EXPECT_EQ(lcd(window)->input(), "A");
}

TEST(MainWindow, AnyNameCanBeAssignedWithTheLetters) {
    MainWindow window;
    openSection(window, "letters");
    for (const char* name : {"direct:letterR", "direct:letterA", "direct:letterT", "direct:letterE"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    openSection(window, "variables");
    for (const char* name : {"direct:assign", "key:2", "key:1"}) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "rate:=21");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "21");
}

TEST(MainWindow, StoreKeepsThePhoneDrawerOpenForTheLetter) {
    MainWindow window;
    window.resize(390, 844);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(390, 844));
    auto* toggle = child<QPushButton>(window, "drawerToggle");
    QTest::mouseClick(toggle, Qt::LeftButton);
    openSection(window, "variables");
    QTest::mouseClick(child<QPushButton>(window, "direct:store"), Qt::LeftButton);
    EXPECT_TRUE(toggle->isChecked());  // the letter is in the drawer too
}

TEST(MainWindow, AWordRemainderIsTypedOrKeyedLetterByLetter) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "7 mod 3");
    EXPECT_EQ(lcd(window)->input(), "7 mod 3");  // letters stay letters; the engine reads the word
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1");
    lcd(window)->clear();
    openSection(window, "letters");
    for (const char* name : {"key:7", "direct:space", "direct:letterR", "direct:letterE", "direct:letterM", "direct:space", "key:3"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "7 rem 3");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1");
}

TEST(MainWindow, ANotationConversionKeepsTheBar) {
    MainWindow window;
    run(window, "0.1 to sci");
    EXPECT_EQ(lcd(window)->outputText(), "1.000000000000000|055511151231257827021181583404541015625×10^−1");
    run(window, "0.1 to fraction");
    EXPECT_EQ(lcd(window)->outputText(), "3602879701896397/36028797018963968");
}

TEST(MainWindow, AnApproximateConversionSaysHowFarUnderTheScreen) {
    MainWindow window;
    run(window, "2.7 to 1/3");
    EXPECT_EQ(lcd(window)->outputText(), "8/3");
    EXPECT_EQ(message(window)->text(), "≈: off by 3.3e-2");
    EXPECT_TRUE(message(window)->property("dimmed").toBool());
    run(window, "2.5 to 1/2");
    EXPECT_EQ(message(window)->text(), "");  // exact: nothing to say
}

TEST(MainWindow, TheReadingShowsWhileTyping) {
    MainWindow window;
    QTest::keyClicks(lcd(window), "2^3^2");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->readingText() == "(2 ^ (3 ^ 2))"; }, 5000))
        << lcd(window)->readingText().toStdString();
    lcd(window)->clear();
    QTest::keyClicks(lcd(window), "7");
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->readingText().isEmpty(); }, 5000));  // nothing to add
}

TEST(MainWindow, FunctionKeysTypeTheirNames) {
    MainWindow window;
    openSection(window, "rounding");
    for (const char* name : {"direct:floor", "key:2", "key:point", "key:7", "key:close"})
        QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "floor(2.7)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "2");
}

TEST(MainWindow, SpecialFunctionKeysTypeTheirNames) {
    MainWindow window;
    openSection(window, "special");
    for (const char* name : {"direct:gamma", "key:5", "key:close"}) QTest::mouseClick(child<QPushButton>(window, name), Qt::LeftButton);
    EXPECT_EQ(lcd(window)->input(), "gamma(5)");
    forget(window);
    QTest::keyClick(lcd(window), Qt::Key_Return);
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "24");
}

TEST(MainWindow, EveryKeySaysWhatItIs) {
    MainWindow window;
    QList<QPair<QString, Key>> keys;
    for (const QList<Key>& row : keypad())
        for (const Key& key : row) keys.append({"key:", key});
    for (const Key& key : everyDirectKey()) keys.append({"direct:", key});
    for (const QString& id : window.common()) keys.append({"common:", directKey(id)});
    for (const auto& [prefix, key] : keys) {
        auto* button = child<QPushButton>(window, (prefix + key.id).toUtf8().constData());
        EXPECT_FALSE(keyTip(key).isEmpty()) << key.id.toStdString();
        EXPECT_TRUE(button->toolTip().startsWith(keyTip(key))) << key.id.toStdString();  // then, on some, how to reach more
    }
    EXPECT_EQ(keyTip(directKey("asin")), "Inverse sine · asin(0.5)");  // the title, then the example
    child<TypeChooser>(window, "type")->setCurrentType(calculate_core::NumberType::Exact);
    EXPECT_TRUE(child<QPushButton>(window, "key:sin")->toolTip().startsWith("Exact arithmetic cannot represent"));
}
