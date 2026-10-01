#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "keypad.hpp"
#include "keysizing.hpp"
#include "lcd.hpp"
#include "typechooser.hpp"
#include "presenter.hpp"
#include "worker.hpp"

#include <QComboBox>
#include <QDialog>
#include <QCursor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

using namespace calculate_core;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), types_(numberTypes()) {
    setWindowTitle(tr("calculate"));
    auto* central = new QWidget(this);
    auto* outer = new QHBoxLayout(central);

    // The left panel: ☰ collapses it to a thin rail; ⚙ (settings) stays at its bottom either way.
    auto* rail = new QWidget(central);
    rail->setObjectName("rail");
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 0, 0, 0);
    auto* panelToggle = new QToolButton(rail);
    panelToggle->setObjectName("panelToggle");
    panelToggle->setText(QStringLiteral("☰"));
    panelToggle->setToolTip(tr("Show or hide the panel"));
    panelToggle->setCheckable(true);
    panelToggle->setAutoRaise(true);
    modes_ = new QListWidget(rail);
    modes_->setObjectName("modes");
    modes_->addItems({tr("Calculator"), tr("Statistics")});
    modes_->setFixedWidth(140);
    auto* settingsButton = new QToolButton(rail);
    settingsButton->setObjectName("settingsButton");
    settingsButton->setText(QStringLiteral("⚙"));
    settingsButton->setToolTip(tr("Settings"));
    settingsButton->setAutoRaise(true);
    railLayout->addWidget(panelToggle, 0, Qt::AlignLeft);
    railLayout->addWidget(modes_, 1);
    railLayout->addStretch();  // keeps ⚙ at the bottom while the list is hidden
    railLayout->addWidget(settingsButton, 0, Qt::AlignLeft);
    outer->addWidget(rail);
    connect(panelToggle, &QToolButton::toggled, modes_, [this](bool collapsed) { modes_->setVisible(!collapsed); });
    settings_ = new QDialog(this);
    settings_->setObjectName("settings");
    settings_->setWindowTitle(tr("Settings"));
    connect(settingsButton, &QToolButton::clicked, settings_, &QDialog::open);

    auto* main = new QVBoxLayout;
    outer->addLayout(main, 1);

    // The screen, and beside it the angle unit, the number type and = stacked to its height.
    auto* screenRow = new QHBoxLayout;
    lcd_ = new Lcd(central);
    lcd_->setObjectName("lcd");
    screenRow->addWidget(lcd_, 1);
    auto* column = new QVBoxLayout;
    angle_ = new QComboBox(central);
    angle_->setObjectName("angle");
    angle_->addItems({tr("RAD"), tr("DEG"), tr("GRAD")});
    type_ = new TypeChooser(central);
    type_->setObjectName("type");
    auto* equals = new QPushButton(tr("="), central);
    equals->setObjectName("equals");
    int width = 0;
    for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals)})
        width = qMax(width, w->sizeHint().width());
    for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals)}) {
        w->setFixedWidth(width);
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        column->addWidget(w);
    }
    screenRow->addLayout(column);
    main->addLayout(screenRow);

    // Along the screen's bottom edge: Details, and the messages that need an answer.
    detailsButton_ = new QToolButton(lcd_);
    detailsButton_->setObjectName("detailsButton");
    detailsButton_->setText(tr("Details"));
    detailsButton_->setAutoRaise(true);
    detailsButton_->setEnabled(false);
    lcd_->addToBar(detailsButton_);
    proceed_ = new QPushButton(tr("Proceed anyway"), lcd_);
    proceed_->setObjectName("proceed");
    proceed_->setVisible(false);
    lcd_->addToBar(proceed_);
    busy_ = new QLabel(tr("Computing…"), lcd_);
    busy_->setObjectName("busy");
    cancel_ = new QPushButton(tr("Cancel"), lcd_);
    cancel_->setObjectName("cancel");
    busy_->setVisible(false);
    cancel_->setVisible(false);
    lcd_->addToBar(busy_);
    lcd_->addToBar(cancel_);
    card_ = new DetailsCard(this);
    card_->setObjectName("detailsCard");

    // The session's history: a list that drops down under the screen from the ▾ at its corner.
    historyToggle_ = new QToolButton(lcd_);
    historyToggle_->setObjectName("historyToggle");
    historyToggle_->setText(QStringLiteral("▾"));
    historyToggle_->setToolTip(tr("History"));
    historyToggle_->setAutoRaise(true);
    historyToggle_->setEnabled(false);
    lcd_->addToBar(historyToggle_, true);
    historyPanel_ = new QFrame(this, Qt::Popup);
    historyPanel_->setObjectName("historyPanel");
    historyPanel_->setFrameShape(QFrame::StyledPanel);
    auto* historyLayout = new QVBoxLayout(historyPanel_);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    history_ = new QListWidget(historyPanel_);
    history_->setObjectName("history");
    historyLayout->addWidget(history_);

    pages_ = new QStackedWidget(central);
    pages_->setObjectName("pages");
    // The keys keep one size (see sizeKeys): centred in a bigger window, scrolled in a smaller one.
    keys_ = new QScrollArea(pages_);
    keys_->setObjectName("keys");
    keys_->setFrameShape(QFrame::NoFrame);
    keys_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    auto* keysArea = new QWidget;
    auto* keysLayout = new QHBoxLayout(keysArea);
    keysLayout->setContentsMargins(0, 0, 0, 0);
    keysLayout->setSpacing(keypadGap);
    keysLayout->addWidget(buildDirectKeys(), 0, Qt::AlignTop);
    keysLayout->addWidget(buildKeypad(), 0, Qt::AlignTop);
    keys_->setWidget(keysArea);
    pages_->addWidget(keys_);
    auto* statistics = new QScrollArea(pages_);  // so neither page sets a minimum width for the window
    statistics->setFrameShape(QFrame::NoFrame);
    statistics->setWidgetResizable(true);
    statistics->setWidget(buildStatistics());
    pages_->addWidget(statistics);
    main->addWidget(pages_, 1);
    setCentralWidget(central);
    modes_->setCurrentRow(0);

    worker_ = new Worker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &MainWindow::evaluationRequested, worker_, &Worker::evaluate);
    connect(this, &MainWindow::memoryAddRequested, worker_, &Worker::memoryAdd);
    connect(this, &MainWindow::memorySubtractRequested, worker_, &Worker::memorySubtract);
    connect(this, &MainWindow::memoryClearRequested, worker_, &Worker::memoryClear);
    connect(worker_, &Worker::evaluated, this, &MainWindow::showResult);
    connect(worker_, &Worker::memoryChanged, lcd_, &Lcd::setMemory);
    connect(worker_, &Worker::memoryFailed, this, [this] { lcd_->showMessage(tr("The memory needs a previous result")); });
    thread_.start();

    busyTimer_.setSingleShot(true);
    busyTimer_.setInterval(300);
    connect(&busyTimer_, &QTimer::timeout, this, [this] {
        busy_->setVisible(true);
        cancel_->setVisible(true);
    });
    connect(cancel_, &QPushButton::clicked, this, [this] { worker_->cancelFlag() = true; });
    connect(modes_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(lcd_, &Lcd::evaluateRequested, this, &MainWindow::evaluate);
    connect(lcd_, &Lcd::historyRequested, this, [this](int step) { replay(historyIndex_ + step); });
    connect(equals, &QPushButton::clicked, this, &MainWindow::evaluate);
    connect(proceed_, &QPushButton::clicked, this, [this] { request(lastExpression_, true); });
    connect(detailsButton_, &QToolButton::clicked, this, [this] { card_->popUp(lcd_); });
    connect(historyToggle_, &QToolButton::clicked, this, [this] {
        historyPanel_->setFixedWidth(lcd_->width());
        historyPanel_->move(lcd_->mapToGlobal(QPoint(0, lcd_->height())));
        historyPanel_->show();
    });
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        lcd_->setInput(item->data(Qt::UserRole).toString());
        historyPanel_->hide();
    });
    auto reevaluate = [this] {
        updateKeys();
        if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, reevaluate);
    buildMenus();
    updateKeys();
    // Only the screen takes the keyboard; every other control is used with the mouse.
    for (QWidget* w : {static_cast<QWidget*>(modes_), static_cast<QWidget*>(panelToggle), static_cast<QWidget*>(settingsButton), static_cast<QWidget*>(type_), static_cast<QWidget*>(angle_),
                       static_cast<QWidget*>(equals), static_cast<QWidget*>(detailsButton_), static_cast<QWidget*>(proceed_),
                       static_cast<QWidget*>(cancel_), static_cast<QWidget*>(historyToggle_)})
        w->setFocusPolicy(Qt::NoFocus);
    lcd_->setFocus();
}

MainWindow::~MainWindow() {
    worker_->cancelFlag() = true;
    thread_.quit();
    thread_.wait();
}

QWidget* MainWindow::buildKeypad() {
    auto* pad = new QWidget;
    pad->setObjectName("keypad");
    auto* layout = new QVBoxLayout(pad);
    auto* functions = new QGridLayout;  // six columns, and four around the cursor pad
    auto* numbers = new QGridLayout;    // five columns, as on the calculator
    functions->setSpacing(keySpacing);
    numbers->setSpacing(keySpacing);
    layout->addLayout(functions);
    layout->addSpacing(keypadGap);
    layout->addLayout(numbers);
    int functionRow = 0, numberRow = 0;
    for (const QList<Key>& row : keypad()) {
        const bool numberKeys = row.size() == 5;
        for (int c = 0; c < row.size(); ++c) {
            if (numberKeys) {
                numbers->addWidget(buildKey(row[c]), numberRow, c);
            } else {
                const int column = row.size() == 4 && c >= 2 ? c + 2 : c;  // columns 2–3 hold the cursor pad
                functions->addWidget(buildKey(row[c]), functionRow, column);
            }
        }
        if (numberKeys) ++numberRow;
        else ++functionRow;
    }
    auto* cursor = new QWidget(pad);
    cursor->setObjectName("cursorPad");
    auto* cross = new QGridLayout(cursor);
    cross->setContentsMargins(0, 0, 0, 0);
    const int places[][2] = {{0, 1}, {1, 0}, {1, 2}, {2, 1}};  // up, left, right, down
    for (int i = 0; i < cursorPad().size(); ++i) cross->addWidget(buildKey(cursorPad()[i], false), places[i][0], places[i][1]);
    functions->addWidget(cursor, 0, 2, 2, 2);
    return pad;
}

QWidget* MainWindow::buildKey(const Key& key, bool legends) {
    auto* cell = new QWidget;
    auto* layout = new QVBoxLayout(cell);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    // SHIFT legends sit on the left and ALPHA legends on the right, so colour is never the only cue.
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    auto legend = [&](const QString& name, const Face& face, const char* light, const char* onDark) {
        auto* l = new QLabel(translated(face.label), cell);
        l->setObjectName(name + ":" + key.id);
        QFont small = l->font();
        small.setPointSizeF(small.pointSizeF() * 0.8);
        l->setFont(small);
        l->setStyleSheet(QStringLiteral("color:%1").arg(dark ? onDark : light));
        l->setMinimumWidth(1);  // a long legend may shrink, but never widens its key
        return l;
    };
    if (legends) {
        auto* legendRow = new QHBoxLayout;
        legendRow->addWidget(legend("shift", key.shift, "#9a6700", "#e3b341"));
        legendRow->addStretch();
        legendRow->addWidget(legend("alpha", key.alpha, "#c62828", "#ff7b72"));
        layout->addLayout(legendRow);
    }

    auto* button = new QPushButton(translated(key.main.label), cell);
    button->setObjectName("key:" + key.id);
    button->setMinimumWidth(32);
    button->setFocusPolicy(Qt::NoFocus);  // the keyboard always stays with the screen  // below the style's default, so every column can be equally wide
    layout->addWidget(button);
    if (key.main.action == KeyAction::Shift || key.main.action == KeyAction::Alpha) {
        button->setCheckable(true);
        if (key.main.action == KeyAction::Shift) shift_ = button;
        else alpha_ = button;
        connect(button, &QPushButton::toggled, this, [this, button](bool on) {
            QPushButton* other = button == shift_ ? alpha_ : shift_;
            if (on) other->setChecked(false);  // SHIFT and ALPHA are never on together
            lcd_->setStatus(shift_->isChecked(), alpha_->isChecked());
            updateKeys();
        });
    } else {
        connect(button, &QPushButton::clicked, this, [this, key] { press(key); });
    }
    return cell;
}

// The direct keys, a titled grid of six columns per topic.
QWidget* MainWindow::buildDirectKeys() {
    auto* keyboard = new QWidget;
    keyboard->setObjectName("directKeys");
    auto* layout = new QVBoxLayout(keyboard);
    for (const KeyGroup& group : directKeys()) {
        auto* title = new QLabel(translated(group.title), keyboard);
        title->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(title);
        auto* grid = new QGridLayout;
        grid->setSpacing(keySpacing);
        for (int i = 0; i < group.keys.size(); ++i) {
            const Key& key = group.keys[i];
            auto* button = new QPushButton(translated(key.main.label), keyboard);
            button->setObjectName("direct:" + key.id);
            button->setMinimumWidth(32);
            button->setFocusPolicy(Qt::NoFocus);
            connect(button, &QPushButton::clicked, this, [this, key] {
                shift_->setChecked(false);  // a direct key is its own function: it ends SHIFT and ALPHA
                alpha_->setChecked(false);
                apply(key.main);
            });
            grid->addWidget(button, i / 6, i % 6);
        }
        grid->setColumnStretch(6, 1);  // shorter rows line up on the left, in the same columns
        layout->addLayout(grid);
    }
    layout->addStretch();
    return keyboard;
}

// Gives every key the size keySize() derives from the window's screen; called when the window is
// first shown and whenever it moves to another screen. What the window reserves for everything but
// the keys is measured from its own layouts, so it holds for any style, font and language.
void MainWindow::sizeKeys() {
    if (!screen()) return;
    QWidget* area = keys_->widget();
    QWidget* pad = findChild<QWidget*>("keypad");
    QWidget* direct = findChild<QWidget*>("directKeys");
    QList<Key> all;
    for (const QList<Key>& row : keypad()) all += row;
    for (const KeyGroup& group : directKeys()) all += group.keys;
    int labels = 0;  // the widest main label, so no key is too narrow for its own name
    for (const Key& key : all) labels = qMax(labels, fontMetrics().horizontalAdvance(translated(key.main.label)));
    const QSize minimum(labels + 10, fontMetrics().height() + 10);

    const QMargins outer = centralWidget()->layout()->contentsMargins();
    const QMargins inner = pad->layout()->contentsMargins();
    const QMargins left = direct->layout()->contentsMargins();
    const int scrollBar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int legend = findChild<QLabel*>("shift:sin")->sizeHint().height();
    const int rows = 9;
    const QSize reserved(outer.left() + outer.right() + centralWidget()->layout()->spacing() + modes_->width()
                             + inner.left() + inner.right() + left.left() + left.right() + area->layout()->spacing()
                             + 2 * keys_->frameWidth() + scrollBar,
                         outer.top() + outer.bottom() + lcd_->minimumSizeHint().height() + 2 * keySpacing
                             + inner.top() + inner.bottom() + keypadGap + rows * legend
                             + style()->pixelMetric(QStyle::PM_TitleBarHeight));
    const QSize size = keySize(screen()->availableGeometry().size(), reserved, QSize(12, rows), keySpacing, minimum);
    const QSize numberSize((6 * size.width() + keySpacing) / 5, size.height());  // five span six

    for (const QList<Key>& row : keypad())
        for (const Key& key : row) {
            auto* button = findChild<QPushButton*>("key:" + key.id);
            button->setFixedSize(row.size() == 5 ? numberSize : size);
            button->parentWidget()->setFixedWidth(button->width());  // the key's cell, legends included
        }
    for (const KeyGroup& group : directKeys())
        for (const Key& key : group.keys) findChild<QPushButton*>("direct:" + key.id)->setFixedSize(size);
    findChild<QWidget*>("cursorPad")->setFixedWidth(2 * size.width() + keySpacing);
    for (QWidget* w : {pad, direct, area}) {
        w->layout()->activate();
        w->adjustSize();
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (keysSized_) return;
    keysSized_ = true;
    sizeKeys();
    connect(windowHandle(), &QWindow::screenChanged, this, &MainWindow::sizeKeys);
}

// MENU picks the mode, SHIFT MENU (CONFIG) the angle unit, and OPTN offers optionsMenu().
void MainWindow::buildMenus() {
    modeMenu_ = new QMenu(this);
    modeMenu_->setObjectName("menu");
    for (int i = 0; i < modes_->count(); ++i)
        connect(modeMenu_->addAction(modes_->item(i)->text()), &QAction::triggered, this, [this, i] { modes_->setCurrentRow(i); });
    configMenu_ = new QMenu(this);
    configMenu_->setObjectName("config");
    for (int i = 0; i < angle_->count(); ++i)
        connect(configMenu_->addAction(angle_->itemText(i)), &QAction::triggered, this, [this, i] { angle_->setCurrentIndex(i); });
    optionsMenu_ = new QMenu(this);
    optionsMenu_->setObjectName("options");
    for (const Face& f : optionsMenu())
        connect(optionsMenu_->addAction(translated(f.label)), &QAction::triggered, this, [this, f] { apply(f); });
    connect(optionsMenu_, &QMenu::aboutToShow, this, [this] {
        const QList<QAction*> actions = optionsMenu_->actions();
        for (int i = 0; i < actions.size(); ++i) actions[i]->setEnabled(available(optionsMenu()[i], exactType()));
    });
}

QWidget* MainWindow::buildStatistics() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Values (one per line, or separated by commas):"), page));
    statisticsValues_ = new QPlainTextEdit(page);
    statisticsValues_->setObjectName("statisticsValues");
    layout->addWidget(statisticsValues_, 1);
    auto* buttons = new QHBoxLayout;
    for (const char* f : {"mean", "median", "var", "stdev", "varp", "stdevp"}) {
        auto* b = new QPushButton(QString::fromLatin1(f), page);
        b->setObjectName(QStringLiteral("stat:") + f);
        buttons->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, f] {
            const QString e = view::statisticsExpression(QString::fromLatin1(f), statisticsValues_->toPlainText());
            if (e.isEmpty()) return;
            lcd_->setInput(e);
            evaluate();
        });
    }
    layout->addLayout(buttons);
    return page;
}

Options MainWindow::options() const {
    Options o;
    o.type = static_cast<NumberType>(type_->currentIndex());
    o.angle = static_cast<AngleUnit>(angle_->currentIndex());
    return o;
}

void MainWindow::evaluate() {
    const QString text = lcd_->input().trimmed();
    if (!text.isEmpty()) request(text, false);
}

void MainWindow::request(const QString& expression, bool allowUncertain) {
    lastExpression_ = expression;
    Options o = options();
    o.allowUncertainDiscreteArguments = allowUncertain;
    ++pending_;
    busyTimer_.start();
    emit evaluationRequested(expression, o);
}

void MainWindow::showResult(const QString& expression, const Result& result) {
    if (--pending_ == 0) {  // an earlier answer must not hide Cancel while a later request runs
        busyTimer_.stop();
        busy_->setVisible(false);
        cancel_->setVisible(false);
    }
    last_ = result;
    lastExpression_ = expression;
    proceed_->setVisible(result.error && result.error->code == ErrorCode::UncertainDiscreteArgument);
    card_->setRows(view::details(result, types_[static_cast<std::size_t>(result.type)]));
    detailsButton_->setEnabled(!result.error);
    if (result.error) {
        lcd_->showMessage(view::errorText(*result.error, expression));
        return;
    }
    if (result.exact) lcd_->showExact(view::fractionParts(result));
    else lcd_->showValue(view::valueParts(result));
    if (history_->count() == 0 || history_->item(0)->data(Qt::UserRole).toString() != expression) {
        // "expression = value", the value cut short: the list only points back to the calculation.
        QString value = lcd_->outputText();
        if (value.size() > 28) value = value.left(28) + QStringLiteral("…");
        auto* item = new QListWidgetItem(expression + QStringLiteral(" = ") + value);
        item->setData(Qt::UserRole, expression);
        history_->insertItem(0, item);
        historyToggle_->setEnabled(true);
    }
    historyIndex_ = -1;
}

const Face& MainWindow::face(const Key& key) const {
    return shift_->isChecked() ? key.shift : alpha_->isChecked() ? key.alpha : key.main;
}

// Like the calculator, SHIFT and ALPHA apply to the next key only.
void MainWindow::press(const Key& key) {
    const Face& f = face(key);
    shift_->setChecked(false);
    alpha_->setChecked(false);
    apply(f);
}

void MainWindow::apply(const Face& f) {
    switch (f.action) {
    case KeyAction::Insert: lcd_->insert(translated(f.insert)); break;
    case KeyAction::Clear: lcd_->clear(); break;
    case KeyAction::Backspace: lcd_->backspace(); break;
    case KeyAction::Evaluate: evaluate(); break;
    case KeyAction::MemoryAdd: emit memoryAddRequested(); break;
    case KeyAction::MemorySubtract: emit memorySubtractRequested(); break;
    case KeyAction::MemoryClear: emit memoryClearRequested(); break;
    case KeyAction::Menu: popUp(modeMenu_); break;
    case KeyAction::Config: popUp(configMenu_); break;
    case KeyAction::Options: popUp(optionsMenu_); break;
    case KeyAction::Left: lcd_->left(); break;
    case KeyAction::Right: lcd_->right(); break;
    case KeyAction::Up: replay(historyIndex_ + 1); break;
    case KeyAction::Down: replay(historyIndex_ - 1); break;
    case KeyAction::Shift:
    case KeyAction::Alpha:
    case KeyAction::Unavailable: break;
    }
    lcd_->setFocus();
}

void MainWindow::popUp(QMenu* menu) { menu->popup(QCursor::pos()); }

// ▲ and ▼ step through the history, newest first, as the calculator's replay does.
void MainWindow::replay(int index) {
    if (index < 0 || index >= history_->count()) return;
    historyIndex_ = index;
    lcd_->setInput(history_->item(index)->data(Qt::UserRole).toString());
}

bool MainWindow::exactType() const { return static_cast<NumberType>(type_->currentIndex()) == NumberType::Exact; }

QString MainWindow::exactRefusal(const QString& label) const {
    return tr("Exact arithmetic cannot represent %1: its result is irrational. Switch to a floating type to compute it.")
        .arg(translated(label));
}

// Enables the keys whose current face (plain, SHIFT or ALPHA) works in the selected type.
void MainWindow::updateKeys() {
    const bool exact = exactType();
    QList<Key> keys = cursorPad();
    for (const QList<Key>& row : keypad()) keys += row;
    for (const Key& key : keys) {
        if (key.main.action == KeyAction::Shift || key.main.action == KeyAction::Alpha) continue;
        auto* button = findChild<QPushButton*>("key:" + key.id);
        const Face& f = face(key);
        const bool on = available(f, exact);
        button->setEnabled(on);
        QString tip;
        if (!on && f.action == KeyAction::Unavailable && !f.label.isEmpty())
            tip = tr("%1 is not available in this app yet").arg(translated(f.label));
        else if (!on && f.action != KeyAction::Unavailable)
            tip = exactRefusal(f.label);
        button->setToolTip(tip);
    }
    for (const KeyGroup& group : directKeys())
        for (const Key& key : group.keys) {
            auto* button = findChild<QPushButton*>("direct:" + key.id);
            const bool on = available(key.main, exact);
            button->setEnabled(on);
            button->setToolTip(on ? QString() : exactRefusal(key.main.label));
        }
}
