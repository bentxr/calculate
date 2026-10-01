#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "lcd.hpp"
#include "presenter.hpp"
#include "typechooser.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

namespace {

template <class W>
W* child(MainWindow& window, const char* name) {
    W* w = window.findChild<W*>(name);
    EXPECT_NE(w, nullptr) << name;
    return w;
}

Lcd* lcd(MainWindow& window) { return child<Lcd>(window, "lcd"); }

// Waits for the worker's answer on the screen.
bool answered(MainWindow& window) {
    return QTest::qWaitFor([&] { return !lcd(window)->outputText().isEmpty(); }, 10000);
}

// Enters an expression, presses Enter and waits for the answer.
void run(MainWindow& window, const QString& expression) {
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
    EXPECT_EQ(type->itemText(1), "Double");
    EXPECT_EQ(type->itemText(6), "Hexadecuple");
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
    lcd(window)->showMessage({});
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    EXPECT_TRUE(answered(window));
    EXPECT_EQ(lcd(window)->outputText(), "1/3 = 0.(3)");
    EXPECT_EQ(detail(window, "exact"), "exact · no rounding error");
}

TEST(MainWindow, ExactModeGreysOutTranscendentalKeys) {
    MainWindow window;
    auto* sin = child<QPushButton>(window, "key:sin");
    EXPECT_TRUE(sin->isEnabled());
    child<QComboBox>(window, "type")->setCurrentIndex(3);
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
    lcd(window)->showMessage({});
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
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    lcd(window)->setInput("1 + 1");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    lcd(window)->setInput("200000!");
    QTest::keyClick(lcd(window), Qt::Key_Return);
    auto* cancel = child<QPushButton>(window, "cancel");
    ASSERT_TRUE(QTest::qWaitFor([&] { return cancel->isVisibleTo(&window); }, 5000));
    QTest::mouseClick(cancel, Qt::LeftButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return lcd(window)->outputText() == "Cancelled"; }, 10000))
        << lcd(window)->outputText().toStdString();
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
        QTest::mouseClick(info, Qt::LeftButton);
        auto* tip = card->findChild<QFrame*>("explanation");
        ASSERT_NE(tip, nullptr);
        EXPECT_TRUE(tip->isVisible()) << key;
        EXPECT_EQ(tip->findChild<QLabel*>()->text(), view::explanation(key));
        EXPECT_LE(qAbs(tip->mapToGlobal(QPoint(0, 0)).y() - info->mapToGlobal(QPoint(0, info->height())).y()), 2);  // under its ⓘ
        EXPECT_EQ(card->size(), size);  // the card itself never changes
        tip->hide();
    }
    card->hide();
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
    child<QComboBox>(window, "type")->setCurrentIndex(3);  // Exact
    EXPECT_FALSE(child<QPushButton>(window, "direct:sinh")->isEnabled());
    EXPECT_FALSE(child<QPushButton>(window, "direct:sinh")->toolTip().isEmpty());
    EXPECT_TRUE(child<QPushButton>(window, "direct:mod")->isEnabled());
}

TEST(MainWindow, DirectKeysSitLeftOfTheCasioPadAtTheSameSize) {
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

TEST(MainWindow, TheSettingsButtonOpensTheSettings) {
    MainWindow window;
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    QTest::mouseClick(child<QToolButton>(window, "settingsButton"), Qt::LeftButton);
    auto* dialog = child<QDialog>(window, "settings");
    EXPECT_TRUE(dialog->isVisible());
    dialog->close();
}

TEST(MainWindow, SpanishRelabelsEverythingWithoutARestart) {
    MainWindow window;
    run(window, "0.1 + 0.2");
    auto* language = child<QComboBox>(window, "language");
    ASSERT_EQ(language->count(), 3);  // System, English, Español
    language->setCurrentIndex(2);
    QCoreApplication::processEvents();  // Qt posts the language change to every window
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->text(), "sen");
    EXPECT_EQ(child<QPushButton>(window, "direct:asin")->text(), "Arcsen");
    EXPECT_EQ(child<QPushButton>(window, "direct:gcd")->text(), "MCD");
    EXPECT_EQ(child<QListWidget>(window, "modes")->item(0)->text(), "Calculadora");
    EXPECT_EQ(child<QComboBox>(window, "type")->itemText(1), "Doble");
    EXPECT_EQ(child<QToolButton>(window, "detailsButton")->text(), "Detalles");
    EXPECT_EQ(child<DetailsCard>(window, "detailsCard")->findChild<QLabel*>("label:bound")->text(), "Cota garantizada");
    EXPECT_EQ(language->itemText(0), "Sistema");
    language->setCurrentIndex(1);
    QCoreApplication::processEvents();
    EXPECT_EQ(child<QPushButton>(window, "key:sin")->text(), "sin");
    EXPECT_EQ(child<QListWidget>(window, "modes")->item(0)->text(), "Calculator");
}

TEST(MainWindow, TheThemeSwitchesBetweenLightAndDark) {
    const QPalette original = QApplication::palette();
    MainWindow window;
    auto* theme = child<QComboBox>(window, "theme");
    ASSERT_EQ(theme->count(), 3);  // System, Light, Dark
    theme->setCurrentIndex(2);
    QCoreApplication::processEvents();
    EXPECT_LT(window.palette().color(QPalette::Window).lightness(), 128);
    EXPECT_LT(lcd(window)->background().lightness(), 80);
    theme->setCurrentIndex(1);
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
    auto* language = child<QComboBox>(window, "language");
    language->setCurrentIndex(2);  // Español
    QCoreApplication::processEvents();
    EXPECT_EQ(geometry(), english);
    language->setCurrentIndex(1);  // English
    QCoreApplication::processEvents();
    EXPECT_EQ(geometry(), english);
}
