#include "mainwindow.hpp"

#include "keypad.hpp"
#include "lcd.hpp"
#include "presenter.hpp"
#include "worker.hpp"

#include <QComboBox>
#include <QCursor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace calculate_core;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), types_(numberTypes()) {
    setWindowTitle(tr("calculate"));
    auto* central = new QWidget(this);
    auto* outer = new QHBoxLayout(central);

    modes_ = new QListWidget(central);
    modes_->setObjectName("modes");
    modes_->addItems({tr("Calculator"), tr("Statistics")});
    modes_->setFixedWidth(140);
    outer->addWidget(modes_);

    auto* main = new QVBoxLayout;
    outer->addLayout(main, 1);

    auto* top = new QHBoxLayout;
    type_ = new QComboBox(central);
    type_->setObjectName("type");
    for (const TypeInfo& t : types_) type_->addItem(view::typeLabel(t));
    type_->setCurrentIndex(static_cast<int>(NumberType::Double));
    angle_ = new QComboBox(central);
    angle_->setObjectName("angle");
    angle_->addItems({tr("RAD"), tr("DEG"), tr("GRAD")});
    top->addWidget(type_, 1);
    top->addWidget(angle_);
    main->addLayout(top);

    auto* entry = new QHBoxLayout;
    lcd_ = new Lcd(central);
    lcd_->setObjectName("lcd");
    auto* equals = new QPushButton(tr("="), central);
    entry->addWidget(lcd_, 1);
    entry->addWidget(equals);
    main->addLayout(entry);

    auto label = [&](const char* name) {
        auto* l = new QLabel(central);
        l->setObjectName(name);
        l->setWordWrap(true);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        main->addWidget(l);
        return l;
    };
    errorLine_ = label("errorLine");
    whyLine_ = label("whyLine");
    detailsToggle_ = new QToolButton(central);
    detailsToggle_->setObjectName("detailsToggle");
    detailsToggle_->setText(tr("Details"));
    detailsToggle_->setCheckable(true);
    main->addWidget(detailsToggle_);
    details_ = label("details");
    details_->setTextFormat(Qt::RichText);
    details_->setVisible(false);
    proceed_ = new QPushButton(tr("Proceed anyway"), central);
    proceed_->setObjectName("proceed");
    proceed_->setVisible(false);
    main->addWidget(proceed_);
    auto* busyRow = new QHBoxLayout;
    busy_ = new QLabel(tr("Computing…"), central);
    busy_->setObjectName("busy");
    cancel_ = new QPushButton(tr("Cancel"), central);
    cancel_->setObjectName("cancel");
    busy_->setVisible(false);
    cancel_->setVisible(false);
    busyRow->addWidget(busy_);
    busyRow->addWidget(cancel_);
    busyRow->addStretch();
    main->addLayout(busyRow);
    memory_ = label("memory");

    pages_ = new QStackedWidget(central);
    pages_->setObjectName("pages");
    auto* calculator = new QWidget(pages_);
    auto* calculatorLayout = new QHBoxLayout(calculator);
    calculatorLayout->addWidget(buildKeypad(), 3);
    history_ = new QListWidget(calculator);
    history_->setObjectName("history");
    calculatorLayout->addWidget(history_, 1);
    pages_->addWidget(calculator);
    pages_->addWidget(buildStatistics());
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
    connect(worker_, &Worker::memoryChanged, this, [this](const QString& m) {
        memory_->setText(m.isEmpty() ? QString() : tr("M = %1").arg(m));
    });
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
    connect(detailsToggle_, &QToolButton::toggled, details_, &QWidget::setVisible);
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { lcd_->setInput(item->text()); });
    auto reevaluate = [this] {
        updateKeys();
        if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, reevaluate);
    buildMenus();
    updateKeys();
    // Only the screen takes the keyboard; every other control is used with the mouse.
    for (QWidget* w : {static_cast<QWidget*>(modes_), static_cast<QWidget*>(type_), static_cast<QWidget*>(angle_),
                       static_cast<QWidget*>(equals), static_cast<QWidget*>(detailsToggle_), static_cast<QWidget*>(proceed_),
                       static_cast<QWidget*>(cancel_), static_cast<QWidget*>(history_)})
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
    layout->addLayout(functions);
    layout->addSpacing(12);
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
    for (int c = 0; c < 6; ++c) functions->setColumnStretch(c, 1);
    for (int c = 0; c < 5; ++c) numbers->setColumnStretch(c, 1);
    auto* cursor = new QWidget(pad);
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
            updateKeys();
        });
    } else {
        connect(button, &QPushButton::clicked, this, [this, key] { press(key); });
    }
    return cell;
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
    if (result.error) {
        lcd_->showMessage(view::errorText(*result.error, expression));
        errorLine_->clear();
        whyLine_->clear();
        details_->clear();
        return;
    }
    if (result.exact) lcd_->showExact(view::fractionParts(result));
    else lcd_->showValue(view::valueParts(result));
    errorLine_->setText(view::errorLine(result));
    whyLine_->setText(view::whyLine(result, types_[static_cast<std::size_t>(result.type)]));
    QString table = "<table>";
    for (const view::DetailRow& row : view::details(result, types_[static_cast<std::size_t>(result.type)]))
        table += "<tr><td>" + row.label.toHtmlEscaped() + "&nbsp;&nbsp;</td><td>" + row.value.toHtmlEscaped() + "</td></tr>";
    details_->setText(table + "</table>");
    if (history_->count() == 0 || history_->item(0)->text() != expression) history_->insertItem(0, expression);
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
    lcd_->setInput(history_->item(index)->text());
}

bool MainWindow::exactType() const { return static_cast<NumberType>(type_->currentIndex()) == NumberType::Exact; }

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
            tip = tr("Exact arithmetic cannot represent %1: its result is irrational. "
                     "Switch to a floating type to compute it.").arg(translated(f.label));
        button->setToolTip(tip);
    }
}
