#include "mainwindow.hpp"

#include "keypad.hpp"
#include "presenter.hpp"
#include "worker.hpp"

#include <QComboBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace calculate_core;

namespace {

// Rich text takes an opaque colour, but styles such as Fusion make the placeholder colour translucent.
QColor opaque(const QColor& ink, const QColor& paper) {
    const int a = ink.alpha();
    const auto mix = [a](int i, int p) { return (i * a + p * (255 - a)) / 255; };
    return QColor(mix(ink.red(), paper.red()), mix(ink.green(), paper.green()), mix(ink.blue(), paper.blue()));
}

}  // namespace

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

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto* entry = new QHBoxLayout;
    expression_ = new QLineEdit(central);
    expression_->setObjectName("expression");
    expression_->setFont(mono);
    auto* equals = new QPushButton(tr("="), central);
    entry->addWidget(expression_, 1);
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
    value_ = label("value");
    value_->setFont(mono);
    value_->setTextFormat(Qt::RichText);
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
    message_ = label("message");
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
    connect(worker_, &Worker::memoryFailed, this, [this] { message_->setText(tr("The memory needs a previous result")); });
    thread_.start();

    busyTimer_.setSingleShot(true);
    busyTimer_.setInterval(300);
    connect(&busyTimer_, &QTimer::timeout, this, [this] {
        busy_->setVisible(true);
        cancel_->setVisible(true);
    });
    connect(cancel_, &QPushButton::clicked, this, [this] { worker_->cancelFlag() = true; });
    connect(modes_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(expression_, &QLineEdit::returnPressed, this, &MainWindow::evaluate);
    connect(equals, &QPushButton::clicked, this, &MainWindow::evaluate);
    connect(proceed_, &QPushButton::clicked, this, [this] { request(lastExpression_, true); });
    connect(detailsToggle_, &QToolButton::toggled, details_, &QWidget::setVisible);
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { expression_->setText(item->text()); });
    auto reevaluate = [this] {
        updateKeys();
        if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, reevaluate);
    updateKeys();
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
    button->setMinimumWidth(32);  // below the style's default, so every column can be equally wide
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
            expression_->setText(e);
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
    const QString text = expression_->text().trimmed();
    if (!text.isEmpty()) request(text, false);
}

void MainWindow::request(const QString& expression, bool allowUncertain) {
    lastExpression_ = expression;
    Options o = options();
    o.allowUncertainDiscreteArguments = allowUncertain;
    busyTimer_.start();
    emit evaluationRequested(expression, o);
}

void MainWindow::showResult(const QString& expression, const Result& result) {
    busyTimer_.stop();
    busy_->setVisible(false);
    cancel_->setVisible(false);
    last_ = result;
    lastExpression_ = expression;
    proceed_->setVisible(result.error && result.error->code == ErrorCode::UncertainDiscreteArgument);
    if (result.error) {
        message_->setText(view::errorText(*result.error, expression));
        value_->clear();
        errorLine_->clear();
        whyLine_->clear();
        details_->clear();
        return;
    }
    message_->clear();
    const QString noise = opaque(palette().color(QPalette::PlaceholderText), palette().color(QPalette::Window)).name();
    value_->setText(view::valueHtml(result, noise));
    errorLine_->setText(view::errorLine(result));
    whyLine_->setText(view::whyLine(result, types_[static_cast<std::size_t>(result.type)]));
    QString table = "<table>";
    for (const auto& [name, v] : view::details(result))
        table += "<tr><td>" + name.toHtmlEscaped() + "&nbsp;&nbsp;</td><td>" + v.toHtmlEscaped() + "</td></tr>";
    details_->setText(table + "</table>");
    if (history_->count() == 0 || history_->item(0)->text() != expression) history_->insertItem(0, expression);
}

const Face& MainWindow::face(const Key& key) const {
    return shift_->isChecked() ? key.shift : alpha_->isChecked() ? key.alpha : key.main;
}

// Like the calculator, SHIFT and ALPHA apply to the next key only.
void MainWindow::press(const Key& key) {
    const Face& f = face(key);
    shift_->setChecked(false);
    alpha_->setChecked(false);
    switch (f.action) {
    case KeyAction::Insert: expression_->insert(translated(f.insert)); break;
    case KeyAction::Clear: expression_->clear(); break;
    case KeyAction::Backspace: expression_->backspace(); break;
    case KeyAction::Evaluate: evaluate(); break;
    case KeyAction::MemoryAdd: emit memoryAddRequested(); break;
    case KeyAction::MemorySubtract: emit memorySubtractRequested(); break;
    case KeyAction::Left: expression_->cursorBackward(false); break;
    case KeyAction::Right: expression_->cursorForward(false); break;
    case KeyAction::Shift:
    case KeyAction::Alpha:
    case KeyAction::Menu:
    case KeyAction::Config:
    case KeyAction::Options:
    case KeyAction::Up:
    case KeyAction::Down:
    case KeyAction::Unavailable: break;
    }
    expression_->setFocus();
}

// Enables the keys whose current face (plain, SHIFT or ALPHA) works in the selected type.
void MainWindow::updateKeys() {
    const bool exact = static_cast<NumberType>(type_->currentIndex()) == NumberType::Exact;
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
