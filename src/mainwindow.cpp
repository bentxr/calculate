#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "formulatip.hpp"
#include "keypad.hpp"
#include "keysizing.hpp"
#include "lcd.hpp"
#include "popupplacement.hpp"
#include "presenter.hpp"
#include "settings.hpp"
#include "typechooser.hpp"
#include "worker.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QHelpEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyleOptionComboBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <iterator>

using namespace calculate_core;

namespace {

// The statistics values box: only numbers and their separators get in, typed or pasted.
class ValuesEdit : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;

protected:
    void keyPressEvent(QKeyEvent* event) override {
        const QString text = event->text();
        const bool typing = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (typing && !text.isEmpty() && text.at(0).isPrint() && !onlyNumbers(text)) return;  // Enter, Backspace, Ctrl+V… pass
        QPlainTextEdit::keyPressEvent(event);
    }
    void insertFromMimeData(const QMimeData* source) override {
        QString text = source->text();
        text.remove(QRegularExpression(QStringLiteral("[^0-9.,; \\n-]")));
        insertPlainText(text);
    }

private:
    static bool onlyNumbers(const QString& text) {
        return std::all_of(text.begin(), text.end(), [](QChar c) { return c.isDigit() || QStringLiteral(".,;- ").contains(c); });
    }
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), types_(numberTypes()) {
    auto* central = new QWidget(this);
    auto* outer = new QHBoxLayout(central);

    // The left panel: ☰ collapses it to a thin rail; ⚙ (settings) stays at its bottom either way.
    auto* rail = new QWidget(central);
    rail->setObjectName("rail");
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 0, 0, 0);
    panelToggle_ = new QToolButton(rail);
    panelToggle_->setObjectName("panelToggle");
    panelToggle_->setText(QStringLiteral("☰"));
    panelToggle_->setCheckable(true);
    panelToggle_->setAutoRaise(true);
    modes_ = new QListWidget(rail);
    modes_->setObjectName("modes");
    modes_->addItems({QString(), QString()});  // Calculator, Statistics (see retranslate)
    modes_->setFixedWidth(140);
    settingsButton_ = new QToolButton(rail);
    settingsButton_->setObjectName("settingsButton");
    settingsButton_->setText(QStringLiteral("⚙"));
    settingsButton_->setAutoRaise(true);
    railLayout->addWidget(panelToggle_, 0, Qt::AlignLeft);
    railLayout->addWidget(modes_, 1);
    railLayout->addStretch();  // keeps ⚙ at the bottom while the list is hidden
    railLayout->addWidget(settingsButton_, 0, Qt::AlignLeft);
    outer->addWidget(rail);
    connect(panelToggle_, &QToolButton::toggled, modes_, [this](bool collapsed) { modes_->setVisible(!collapsed); });
    buildSettings();
    connect(settingsButton_, &QToolButton::clicked, this, [this] {
        settings_->popup(placed(settings_->sizeHint(), globalGeometry(settingsButton_), popupBounds(settingsButton_)).topLeft());
    });

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
    angle_->setSizeAdjustPolicy(QComboBox::AdjustToContents);  // its texts change with the language
    angle_->addItems({QString(), QString(), QString()});  // RAD, DEG, GRAD (see retranslate)
    type_ = new TypeChooser(central);
    type_->setObjectName("type");
    type_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    equals_ = new QPushButton(central);
    equals_->setObjectName("equals");
    for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals_)}) {
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);  // as wide as the widest: see retranslate
        column->addWidget(w);
    }
    screenRow->addLayout(column);
    main->addLayout(screenRow);

    // Under the screen, a strip of fixed height for messages (errors, cautions), so that showing one
    // never moves anything; Proceed anyway sits at its end when it applies.
    auto* strip = new QHBoxLayout;
    message_ = new QLabel(central);
    message_->setObjectName("message");
    message_->setWordWrap(true);
    message_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    proceed_ = new QPushButton(central);
    proceed_->setObjectName("proceed");
    proceed_->setVisible(false);
    strip->addWidget(message_, 1);
    strip->addWidget(proceed_, 0, Qt::AlignTop);
    auto* stripHolder = new QWidget(central);
    stripHolder->setLayout(strip);
    strip->setContentsMargins(0, 0, 0, 0);
    stripHolder->setFixedHeight(qMax(2 * fontMetrics().lineSpacing(), proceed_->sizeHint().height()) + 4);
    main->addWidget(stripHolder);

    // Along the screen's bottom edge: Details, and the messages that need an answer.
    detailsButton_ = new QToolButton(lcd_);
    detailsButton_->setObjectName("detailsButton");
    detailsButton_->setAutoRaise(true);
    detailsButton_->setEnabled(false);
    lcd_->addToBar(detailsButton_);
    busy_ = new QLabel(lcd_);
    busy_->setObjectName("busy");
    cancel_ = new QPushButton(lcd_);
    cancel_->setObjectName("cancel");
    busy_->setVisible(false);
    cancel_->setVisible(false);
    lcd_->addToBar(busy_);
    lcd_->addToBar(cancel_);
    card_ = new DetailsCard(this);
    card_->setObjectName("detailsCard");
    formulaTip_ = new FormulaTip(this);
    formulaTip_->setObjectName("formulaTip");

    // The session's history: a list that drops down under the screen from the ▾ at its corner.
    historyToggle_ = new QToolButton(lcd_);
    historyToggle_->setObjectName("historyToggle");
    historyToggle_->setText(QStringLiteral("▾"));
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
    qApp->installEventFilter(new PopupBounds(this));  // Qt's own popups stay inside the window too

    worker_ = new Worker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &MainWindow::evaluationRequested, worker_, &Worker::evaluate);
    connect(this, &MainWindow::memoryAddRequested, worker_, &Worker::memoryAdd);
    connect(this, &MainWindow::memorySubtractRequested, worker_, &Worker::memorySubtract);
    connect(this, &MainWindow::memoryClearRequested, worker_, &Worker::memoryClear);
    connect(worker_, &Worker::evaluated, this, &MainWindow::showResult);
    connect(worker_, &Worker::memoryChanged, lcd_, &Lcd::setMemory);
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
    connect(lcd_, &Lcd::evaluateRequested, this, &MainWindow::evaluate);
    connect(lcd_, &Lcd::historyRequested, this, [this](int step) { replay(historyIndex_ + step); });
    connect(equals_, &QPushButton::clicked, this, &MainWindow::evaluate);
    connect(proceed_, &QPushButton::clicked, this, [this] { request(lastExpression_, true); });
    connect(detailsButton_, &QToolButton::clicked, this, [this] { card_->popUp(lcd_); });
    connect(historyToggle_, &QToolButton::clicked, this, [this] {
        const QSize size(lcd_->width(), historyPanel_->sizeHint().height());
        historyPanel_->setGeometry(placed(size, globalGeometry(lcd_), popupBounds(lcd_)));
        historyPanel_->show();
    });
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        lcd_->setEntry(historyEntries_[static_cast<std::size_t>(history_->row(item))]);
        historyPanel_->hide();
    });
    auto reevaluate = [this] {
        updateKeys();
        if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, reevaluate);
    retranslate();
    // Only the screen takes the keyboard; every other control is used with the mouse.
    for (QWidget* w : {static_cast<QWidget*>(modes_), static_cast<QWidget*>(panelToggle_), static_cast<QWidget*>(settingsButton_),
                       static_cast<QWidget*>(type_), static_cast<QWidget*>(angle_), static_cast<QWidget*>(equals_),
                       static_cast<QWidget*>(detailsButton_), static_cast<QWidget*>(proceed_), static_cast<QWidget*>(cancel_),
                       static_cast<QWidget*>(historyToggle_)})
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
                numbers->addWidget(buildKey(row[c], "key:"), numberRow, c);
            } else {
                const int column = row.size() == 4 && c >= 2 ? c + 2 : c;  // columns 2–3 hold the cursor pad
                functions->addWidget(buildKey(row[c], "key:"), functionRow, column);
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
    for (int i = 0; i < cursorPad().size(); ++i) cross->addWidget(buildKey(cursorPad()[i], "key:"), places[i][0], places[i][1]);
    functions->addWidget(cursor, 0, 2, 2, 2);
    return pad;
}

QPushButton* MainWindow::buildKey(const Key& key, const QString& prefix) {
    auto* button = new QPushButton;
    button->setObjectName(prefix + key.id);
    button->setMinimumWidth(32);          // below the style's default, so every column can be equally wide
    button->setFocusPolicy(Qt::NoFocus);  // the keyboard always stays with the screen
    connect(button, &QPushButton::clicked, this, [this, key] { apply(key.face); });
    return button;
}

// The direct keys, a titled grid of six columns per topic.
QWidget* MainWindow::buildDirectKeys() {
    auto* keyboard = new QWidget;
    keyboard->setObjectName("directKeys");
    auto* layout = new QVBoxLayout(keyboard);
    for (int g = 0; g < directKeys().size(); ++g) {
        const KeyGroup& group = directKeys()[g];
        auto* title = new QLabel(keyboard);
        title->setObjectName("group:" + QString::number(g));
        title->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(title);
        auto* grid = new QGridLayout;
        grid->setSpacing(keySpacing);
        for (int i = 0; i < group.keys.size(); ++i) {
            const Key& key = group.keys[i];
            grid->addWidget(buildKey(key, "direct:"), i / 6, i % 6);
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
    int labels = 0;  // the widest label in any language, so no key is too narrow for its name
    for (const Key& key : all)
        for (const QString& label : settings::inEveryLanguage("keypad", key.face.label))
            labels = qMax(labels, fontMetrics().horizontalAdvance(label));
    const QSize minimum(labels + 10, fontMetrics().height() + 10);

    const QMargins outer = centralWidget()->layout()->contentsMargins();
    const QMargins inner = pad->layout()->contentsMargins();
    const QMargins left = direct->layout()->contentsMargins();
    const int scrollBar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int rows = 7;
    const QSize reserved(outer.left() + outer.right() + centralWidget()->layout()->spacing() + modes_->width()
                             + inner.left() + inner.right() + left.left() + left.right() + area->layout()->spacing()
                             + 2 * keys_->frameWidth() + scrollBar,
                         outer.top() + outer.bottom() + lcd_->minimumSizeHint().height() + 2 * keySpacing
                             + inner.top() + inner.bottom() + keypadGap
                             + style()->pixelMetric(QStyle::PM_TitleBarHeight));
    const QSize size = keySize(screen()->availableGeometry().size(), reserved, QSize(12, rows), keySpacing, minimum);
    const QSize numberSize((6 * size.width() + keySpacing) / 5, size.height());  // five span six

    for (const QList<Key>& row : keypad())
        for (const Key& key : row) {
            findChild<QPushButton*>("key:" + key.id)->setFixedSize(row.size() == 5 ? numberSize : size);
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

// ⚙: a small menu that opens from the button, with the values of the language and of the theme listed
// in place. A choice applies at once and is forgotten at exit.
void MainWindow::buildSettings() {
    settings_ = new QMenu(this);
    settings_->setObjectName("settings");
    // One heading, then one checkable action per value, in the order of the setting's enum.
    const auto addSetting = [this](QAction*& section, std::initializer_list<const char*> values, int checked, auto apply) {
        section = settings_->addSection(QString());
        auto* group = new QActionGroup(settings_);
        int i = 0;
        for (const char* value : values) {
            QAction* action = settings_->addAction(QString());  // the texts: see retranslate
            action->setObjectName(QString::fromLatin1(value));
            action->setCheckable(true);
            action->setChecked(i == checked);
            group->addAction(action);
            connect(action, &QAction::triggered, this, [apply, i] { apply(i); });
            ++i;
        }
    };
    addSetting(languageSection_, {"language:system", "language:en", "language:es"}, 0,
               [](int i) { settings::setLanguage(static_cast<settings::Language>(i)); });
    addSetting(themeSection_, {"theme:system", "theme:light", "theme:dark"}, 0,
               [](int i) { settings::setTheme(static_cast<settings::Theme>(i)); });
}

// Every text of the window in the current language: run once when it is built, and again on every
// QEvent::LanguageChange, so the language can change while the calculator runs.
void MainWindow::retranslate() {
    setWindowTitle(tr("calculate"));
    panelToggle_->setToolTip(tr("Show or hide the panel"));
    settingsButton_->setToolTip(tr("Settings"));
    const QStringList modes{tr("Calculator"), tr("Statistics")};
    for (int i = 0; i < modes.size(); ++i) modes_->item(i)->setText(modes[i]);
    const QStringList angles{tr("RAD"), tr("DEG"), tr("GRAD")};
    for (int i = 0; i < angles.size(); ++i) angle_->setItemText(i, angles[i]);
    type_->retranslate();
    equals_->setText(tr("="));
    detailsButton_->setText(tr("Details"));
    proceed_->setText(tr("Proceed anyway"));
    busy_->setText(tr("Computing…"));
    cancel_->setText(tr("Cancel"));
    historyToggle_->setToolTip(tr("History"));
    statisticsLabel_->setText(tr("Values (one per line, or separated by commas):"));
    statisticsKeysToggle_->setToolTip(tr("Show or hide the keypad"));
    languageSection_->setText(tr("Language"));
    themeSection_->setText(tr("Theme"));
    findChild<QAction*>("language:system")->setText(tr("System"));
    findChild<QAction*>("language:en")->setText(QStringLiteral("English"));  // each language in its own name
    findChild<QAction*>("language:es")->setText(QStringLiteral("Español"));
    findChild<QAction*>("theme:system")->setText(tr("System"));
    findChild<QAction*>("theme:light")->setText(tr("Light"));
    findChild<QAction*>("theme:dark")->setText(tr("Dark"));

    QList<Key> keys = cursorPad();
    for (const QList<Key>& row : keypad()) keys += row;
    for (const Key& key : keys) findChild<QPushButton*>("key:" + key.id)->setText(translated(key.face.label));
    for (int g = 0; g < directKeys().size(); ++g) {
        findChild<QLabel*>("group:" + QString::number(g))->setText(translated(directKeys()[g].title));
        for (const Key& key : directKeys()[g].keys) findChild<QPushButton*>("direct:" + key.id)->setText(translated(key.face.label));
    }

    // angle, type and = share one width, wide enough for their texts in every language
    int text = 0;
    for (const char* angle : {"RAD", "DEG", "GRAD"})
        for (const QString& t : settings::inEveryLanguage("MainWindow", QString::fromLatin1(angle)))
            text = qMax(text, fontMetrics().horizontalAdvance(t));
    for (const TypeInfo& type : types_)
        for (const QString& t : settings::inEveryLanguage("view", view::shortTypeNameSource(type)))
            text = qMax(text, fontMetrics().horizontalAdvance(t));
    QStyleOptionComboBox option;
    option.initFrom(type_);
    const int width = style()->sizeFromContents(QStyle::CT_ComboBox, &option, QSize(text, fontMetrics().height()), type_).width();
    for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals_)})
        w->setFixedWidth(width);

    if (hasResult_) present();
    updateKeys();
    if (keysSized_) sizeKeys();  // labels changed width
}

// Hovering a statistics button shows how that statistic is computed.
bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    const QString statistic = watched->property("statistic").toString();
    if (!statistic.isEmpty() && event->type() == QEvent::ToolTip) {
        formulaTip_->showFor(statistic, static_cast<QHelpEvent*>(event)->globalPos());
        return true;
    }
    if (!statistic.isEmpty() && event->type() == QEvent::Leave) formulaTip_->hide();
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    QMainWindow::changeEvent(event);
}

QWidget* MainWindow::buildStatistics() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* header = new QHBoxLayout;
    statisticsLabel_ = new QLabel(page);
    statisticsKeysToggle_ = new QToolButton(page);
    statisticsKeysToggle_->setObjectName("statisticsKeysToggle");
    statisticsKeysToggle_->setText(QStringLiteral("⌨"));
    statisticsKeysToggle_->setCheckable(true);
    statisticsKeysToggle_->setChecked(true);
    statisticsKeysToggle_->setAutoRaise(true);
    statisticsKeysToggle_->setFocusPolicy(Qt::NoFocus);
    header->addWidget(statisticsLabel_, 1);
    header->addWidget(statisticsKeysToggle_);
    layout->addLayout(header);

    auto* entry = new QHBoxLayout;
    statisticsValues_ = new ValuesEdit(page);
    statisticsValues_->setObjectName("statisticsValues");
    entry->addWidget(statisticsValues_, 1);
    // Beside the values: the functions, and under them a numeric keypad, so the page also works
    // without a keyboard.
    auto* side = new QVBoxLayout;
    auto* functions = new QGridLayout;
    const char* names[] = {"mean", "median", "var", "stdev", "varp", "stdevp"};
    for (int i = 0; i < int(std::size(names)); ++i) {
        const QString f = QString::fromLatin1(names[i]);
        auto* b = new QPushButton(f, page);
        b->setObjectName(QStringLiteral("stat:") + f);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("statistic", f);
        b->installEventFilter(this);  // hovering shows the formula (see eventFilter)
        functions->addWidget(b, i / 3, i % 3);
        connect(b, &QPushButton::clicked, this, [this, f] {
            const QString e = view::statisticsExpression(f, statisticsValues_->toPlainText());
            if (e.isEmpty()) return;
            lcd_->setInput(e);
            evaluate();
        });
    }
    side->addLayout(functions);
    side->addSpacing(keypadGap);
    auto* keys = new QWidget(page);
    keys->setObjectName("statisticsKeys");
    auto* grid = new QGridLayout(keys);
    grid->setContentsMargins(0, 0, 0, 0);
    const struct {
        const char* id;
        const char* label;
        const char* insert;  // empty: delete the character before the cursor
    } pad[] = {{"7", "7", "7"}, {"8", "8", "8"}, {"9", "9", "9"}, {"backspace", "⌫", ""},
               {"4", "4", "4"}, {"5", "5", "5"}, {"6", "6", "6"}, {"next", "⏎", "\n"},
               {"1", "1", "1"}, {"2", "2", "2"}, {"3", "3", "3"}, {"minus", "−", "-"},
               {"0", "0", "0"}, {"point", ".", "."}, {"comma", ",", ", "}};
    for (int i = 0; i < int(std::size(pad)); ++i) {
        auto* button = new QPushButton(QString::fromUtf8(pad[i].label), keys);
        button->setObjectName(QStringLiteral("statKey:") + pad[i].id);
        button->setFocusPolicy(Qt::NoFocus);
        const QString insert = QString::fromUtf8(pad[i].insert);
        connect(button, &QPushButton::clicked, this, [this, insert] {
            if (insert.isEmpty()) statisticsValues_->textCursor().deletePreviousChar();
            else statisticsValues_->insertPlainText(insert);
        });
        grid->addWidget(button, i / 4, i % 4);
    }
    side->addWidget(keys);
    side->addStretch();
    entry->addLayout(side);
    connect(statisticsKeysToggle_, &QToolButton::toggled, keys, &QWidget::setVisible);
    layout->addLayout(entry, 1);
    return page;
}

Options MainWindow::options() const {
    Options o;
    o.type = type_->currentType();
    o.angle = static_cast<AngleUnit>(angle_->currentIndex());
    return o;
}

void MainWindow::evaluate() {
    const QString text = lcd_->input().trimmed();
    if (text.isEmpty()) return;
    typed_ = lcd_->entry();  // the history keeps it as typed, templates and all
    request(text, false);
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
    hasResult_ = true;
    present();
    if (result.error) return;
    if (history_->count() == 0 || history_->item(0)->data(Qt::UserRole).toString() != expression) {
        // "expression = value", the value cut short: the list only points back to the calculation.
        QString value = lcd_->outputText();
        if (value.size() > 28) value = value.left(28) + QStringLiteral("…");
        auto* item = new QListWidgetItem(expression + QStringLiteral(" = ") + value);
        item->setData(Qt::UserRole, expression);
        history_->insertItem(0, item);
        Entry entry = typed_;
        if (entry.text().trimmed() != expression) entry.setText(expression);  // not what was typed last
        historyEntries_.insert(historyEntries_.begin(), entry);
        historyToggle_->setEnabled(true);
    }
    historyIndex_ = -1;
}

// Shows the last result on the screen and in the card, in the current language.
void MainWindow::present() {
    proceed_->setVisible(last_.error && last_.error->code == ErrorCode::UncertainDiscreteArgument);
    card_->setRows(view::details(last_, types_[static_cast<std::size_t>(last_.type)]));
    detailsButton_->setEnabled(!last_.error);
    message_->setText(last_.error ? view::errorText(*last_.error, lastExpression_) : QString());
    if (last_.error) lcd_->clearResult();
    else if (last_.exact) lcd_->showExact(view::fractionParts(last_));
    else lcd_->showValue(view::valueParts(last_));
}

void MainWindow::apply(const Face& f) {
    switch (f.action) {
    case KeyAction::Insert: lcd_->insert(translated(f.insert)); break;
    case KeyAction::Template: lcd_->insertTemplate(f.shape, f.insert); break;
    case KeyAction::Clear: lcd_->clear(); break;
    case KeyAction::Backspace: lcd_->backspace(); break;
    case KeyAction::Evaluate: evaluate(); break;
    case KeyAction::MemoryAdd: emit memoryAddRequested(); break;
    case KeyAction::MemorySubtract: emit memorySubtractRequested(); break;
    case KeyAction::MemoryClear: emit memoryClearRequested(); break;
    case KeyAction::Left: lcd_->left(); break;
    case KeyAction::Right: lcd_->right(); break;
    case KeyAction::Up:
        if (!lcd_->up()) replay(historyIndex_ + 1);
        break;
    case KeyAction::Down:
        if (!lcd_->down()) replay(historyIndex_ - 1);
        break;
    }
    lcd_->setFocus();
}

// ▲ and ▼ step through the history, newest first, as the calculator's replay does.
void MainWindow::replay(int index) {
    if (index < 0 || index >= history_->count()) return;
    historyIndex_ = index;
    lcd_->setEntry(historyEntries_[static_cast<std::size_t>(index)]);
}

bool MainWindow::exactType() const { return type_->currentType() == NumberType::Exact; }

QString MainWindow::exactRefusal(const QString& label) const {
    return tr("Exact arithmetic cannot represent %1: its result is irrational. Switch to a floating type to compute it.")
        .arg(translated(label));
}

// Enables the keys that work in the selected type: Exact refuses the irrational functions.
void MainWindow::updateKeys() {
    const bool exact = exactType();
    QList<QPair<QString, Key>> keys;
    for (const QList<Key>& row : keypad())
        for (const Key& key : row) keys.append({"key:", key});
    for (const KeyGroup& group : directKeys())
        for (const Key& key : group.keys) keys.append({"direct:", key});
    for (const auto& [prefix, key] : keys) {
        auto* button = findChild<QPushButton*>(prefix + key.id);
        const bool on = available(key.face, exact);
        button->setEnabled(on);
        button->setToolTip(on ? QString() : exactRefusal(key.face.label));
    }
}
