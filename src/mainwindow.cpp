#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "formulatip.hpp"
#include "icons.hpp"
#include "keypad.hpp"
#include "keysizing.hpp"
#include "lcd.hpp"
#include "longpress.hpp"
#include "popupplacement.hpp"
#include "presenter.hpp"
#include "settings.hpp"
#include "settingsfile.hpp"
#include "typechooser.hpp"
#include "typing.hpp"
#include "worker.hpp"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
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
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyleOptionComboBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <iterator>

using namespace calculate_core;

namespace {

// Errors that "Proceed anyway" can accept, leaving the bound incomplete.
bool canProceed(ErrorCode code) {
    return code == ErrorCode::UncertainDiscreteArgument || code == ErrorCode::ArgumentNearJump
           || code == ErrorCode::ArgumentNearEdge;
}

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

    // The left panel: ☰ collapses it to a thin rail; the gear (settings) stays at its bottom either way.
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
    settingsButton_->setAutoRaise(true);
    railLayout->addWidget(panelToggle_, 0, Qt::AlignLeft);
    railLayout->addWidget(modes_, 1);
    railLayout->addStretch();  // keeps the gear at the bottom while the list is hidden
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
    // The result in the form wanted: as shown, its trusted digits, with its bound, its details, or the expression.
    copyButton_ = new QToolButton(lcd_);
    copyButton_->setObjectName("copyButton");
    copyButton_->setAutoRaise(true);
    lcd_->addToBar(copyButton_);
    copyMenu_ = new QMenu(this);
    copyMenu_->setObjectName("copyMenu");
    copyMenu_->setToolTipsVisible(true);  // why a form is unavailable
    const std::pair<const char*, view::CopyForm> forms[] = {{"copy:value", view::CopyForm::Value},
                                                            {"copy:trusted", view::CopyForm::Trusted},
                                                            {"copy:bound", view::CopyForm::ValueAndBound},
                                                            {"copy:details", view::CopyForm::Details}};
    for (const auto& [name, form] : forms) {
        QAction* action = copyMenu_->addAction(QString());  // the texts: see retranslate
        action->setObjectName(QString::fromLatin1(name));
        connect(action, &QAction::triggered, this, [this, form = form] {
            const Result& r = shownResult();
            QGuiApplication::clipboard()->setText(view::copyText(r, form, types_[static_cast<std::size_t>(r.type)]));
        });
    }
    QAction* copyExpression = copyMenu_->addAction(QString());
    copyExpression->setObjectName("copy:expression");
    // As text that reads back into the same templates.
    connect(copyExpression, &QAction::triggered, this, [this] { QGuiApplication::clipboard()->setText(shownExpression(lcd_->input())); });
    lcd_->editMenu()->addSeparator();
    copyAsMenu_ = lcd_->editMenu()->addMenu(QString());
    copyAsMenu_->setObjectName("copyAsMenu");
    copyAsMenu_->setToolTipsVisible(true);
    copyAsMenu_->addActions(copyMenu_->actions());
    enableCopy(nullptr);
    // Every edit action within reach of a button: undo, redo, cut, copy, paste, select all.
    editButton_ = new QToolButton(lcd_);
    editButton_->setObjectName("editButton");
    editButton_->setAutoRaise(true);
    lcd_->addToBar(editButton_);
    // The system keyboard on request: a phone shows it only when asked, so the keypad stays in view. On the
    // desktop the keyboard is always there, so the button is only in the browser.
    keyboardButton_ = new QToolButton(lcd_);
    keyboardButton_->setObjectName("keyboardButton");
    keyboardButton_->setAutoRaise(true);
    keyboardButton_->setCheckable(true);
    keyboardButton_->setChecked(lcd_->testAttribute(Qt::WA_InputMethodEnabled));
    lcd_->addToBar(keyboardButton_);
#ifndef Q_OS_WASM
    keyboardButton_->hide();
#endif
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
    // A row's own menu: a right click, or a long press on a touch screen.
    history_->setContextMenuPolicy(Qt::CustomContextMenu);
    historyMenu_ = new QMenu(historyPanel_);  // its own: in the browser a popup hides a later one that is not its child
    historyMenu_->setObjectName("historyMenu");
    const std::pair<const char*, view::CopyForm> historyForms[] = {{"history:copyValue", view::CopyForm::Value},
                                                                   {"history:copyBound", view::CopyForm::ValueAndBound}};
    QAction* copyRowExpression = historyMenu_->addAction(QString());
    copyRowExpression->setObjectName("history:copyExpression");
    connect(copyRowExpression, &QAction::triggered, this, [this] {
        const int row = history_->currentRow();
        if (row >= 0) QGuiApplication::clipboard()->setText(shownExpression(historyEntries_[static_cast<std::size_t>(row)].text()));
    });
    for (const auto& [name, form] : historyForms) {
        QAction* action = historyMenu_->addAction(QString());
        action->setObjectName(QString::fromLatin1(name));
        connect(action, &QAction::triggered, this, [this, form = form] {
            const int row = history_->currentRow();
            if (row < 0) return;
            const Result& r = historyResults_[static_cast<std::size_t>(row)];
            QGuiApplication::clipboard()->setText(view::copyText(r, form, types_[static_cast<std::size_t>(r.type)]));
        });
    }
    connect(history_, &QListWidget::customContextMenuRequested, this, &MainWindow::popUpHistoryMenu);
    connect(new LongPress(history_->viewport()), &LongPress::longPressed, this, &MainWindow::popUpHistoryMenu);

    pages_ = new QStackedWidget(central);
    pages_->setObjectName("pages");
    // The keys keep one size (see sizeKeys): centred in a bigger window, scrolled in a smaller one. The left column
    // scrolls on its own when its open sections make it taller than the window, so the main pad never moves.
    keys_ = new QScrollArea(pages_);
    keys_->setObjectName("keys");
    keys_->setFrameShape(QFrame::NoFrame);
    keys_->setWidgetResizable(true);
    auto* keysArea = new QWidget;
    auto* keysLayout = new QHBoxLayout(keysArea);
    keysLayout->setContentsMargins(0, 0, 0, 0);
    keysLayout->setSpacing(keypadGap);
    directScroll_ = new QScrollArea(keysArea);
    directScroll_->setObjectName("directScroll");
    directScroll_->setFrameShape(QFrame::NoFrame);
    directScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    directScroll_->setWidgetResizable(true);
    directScroll_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    directScroll_->setWidget(buildDirectKeys());
    keysLayout->addStretch();
    keysLayout->addWidget(directScroll_);
    keysLayout->addWidget(buildKeypad(), 0, Qt::AlignTop);
    keysLayout->addStretch();
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
    connect(this, &MainWindow::previewRequested, worker_, &Worker::preview);
    connect(worker_, &Worker::evaluated, this, &MainWindow::showResult);
    connect(worker_, &Worker::previewed, this, &MainWindow::showPreview);
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
    liveTimer_.setSingleShot(true);
    liveTimer_.setInterval(liveDelay);
    connect(&liveTimer_, &QTimer::timeout, this, &MainWindow::requestPreview);
    previewLimit_.setSingleShot(true);
    previewLimit_.setInterval(previewLimit);
    connect(&previewLimit_, &QTimer::timeout, this, [this] {
        dropPreviews();
        showNoPreview(tr("Too long to work out while typing: press = to calculate it"));
    });
    connect(lcd_, &Lcd::inputChanged, this, [this] {
        if (settings::liveCalculation()) liveTimer_.start();
    });
    connect(modes_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(lcd_, &Lcd::evaluateRequested, this, &MainWindow::evaluate);
    connect(lcd_, &Lcd::historyRequested, this, [this](int step) { replay(historyIndex_ + step); });
    connect(equals_, &QPushButton::clicked, this, &MainWindow::evaluate);
    connect(proceed_, &QPushButton::clicked, this, [this] { request(lastExpression_, true); });
    connect(detailsButton_, &QToolButton::clicked, this, [this] { card_->popUp(lcd_); });
    connect(keyboardButton_, &QToolButton::toggled, lcd_, &Lcd::setSystemKeyboard);
    connect(copyButton_, &QToolButton::clicked, this, &MainWindow::popUpCopyMenu);
    connect(lcd_, &Lcd::copyMenuRequested, this, &MainWindow::popUpCopyMenu);
    connect(lcd_, &Lcd::copyRequested, findChild<QAction*>("copy:value"), &QAction::trigger);
    connect(editButton_, &QToolButton::clicked, this, [this] {
        QMenu* menu = lcd_->editMenu();
        menu->popup(placed(menu->sizeHint(), globalGeometry(editButton_), popupBounds(editButton_)).topLeft());
    });
    // The names that complete a typed one: a list over the keys, not a window, so the keyboard stays with the screen.
    completions_ = new QListWidget(centralWidget());
    completions_->setObjectName("completions");
    completions_->setFocusPolicy(Qt::NoFocus);
    completions_->hide();
    connect(lcd_, &Lcd::nameTyped, this, &MainWindow::showCompletions);
    connect(completions_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { chooseCompletion(item->text()); });
    connect(lcd_, &Lcd::completionKey, this, [this](int key) {
        const int row = completions_->currentRow();
        if (key == Qt::Key_Up) completions_->setCurrentRow(qMax(row - 1, 0));
        else if (key == Qt::Key_Down) completions_->setCurrentRow(qMin(row + 1, completions_->count() - 1));
        else if (key == Qt::Key_Escape) hideCompletions();
        else chooseCompletion(completions_->currentItem()->text());  // Tab, Enter: choose, don't evaluate
    });
    connect(lcd_, &Lcd::pastedFirstLine, this, [this](int lines) { message_->setText(tr("Pasted the first of %1 lines").arg(lines)); });
    connect(lcd_, &Lcd::pasteRefused, this,
            [this] { message_->setText(tr("The browser did not allow reading the clipboard: paste with Ctrl+V")); });
    connect(historyToggle_, &QToolButton::clicked, this, [this] {
        const QSize size(lcd_->width(), historyPanel_->sizeHint().height());
        historyPanel_->setGeometry(placed(size, globalGeometry(lcd_), popupBounds(lcd_)));
        historyPanel_->show();
    });
    connect(history_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        lcd_->setEntry(historyEntries_[static_cast<std::size_t>(history_->row(item))]);
        historyPanel_->hide();
    });
    // A new type or angle: what is being typed is worked out again, else the last result.
    auto reevaluate = [this] {
        updateKeys();
        if (settings::liveCalculation() && (previewShown_ || lcd_->input().trimmed() != lastExpression_)) requestPreview();
        else if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    };
    connect(type_, &QComboBox::currentIndexChanged, this, reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, reevaluate);
    retranslate();
    // Only the screen takes the keyboard; every other control is used with the mouse.
    for (QWidget* w : {static_cast<QWidget*>(modes_), static_cast<QWidget*>(panelToggle_), static_cast<QWidget*>(settingsButton_),
                       static_cast<QWidget*>(type_), static_cast<QWidget*>(angle_), static_cast<QWidget*>(equals_),
                       static_cast<QWidget*>(detailsButton_), static_cast<QWidget*>(editButton_), static_cast<QWidget*>(keyboardButton_),
                       static_cast<QWidget*>(proceed_), static_cast<QWidget*>(cancel_), static_cast<QWidget*>(historyToggle_)})
        w->setFocusPolicy(Qt::NoFocus);
    drawIcons();
    defaults_ = settingValues();
    lcd_->setFocus();
}

MainWindow::~MainWindow() {
    worker_->cancelFlag() = true;
    worker_->previewCancelFlag() = true;
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

// A grid of keys in six columns; shorter rows line up on the left, in the same columns.
QWidget* MainWindow::keyGrid(const QString& name, const QList<Key>& keys, const QString& prefix) {
    auto* grid = new QWidget;
    grid->setObjectName(name);
    auto* layout = new QGridLayout(grid);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(keySpacing);
    for (int i = 0; i < keys.size(); ++i) layout->addWidget(buildKey(keys[i], prefix), i / 6, i % 6);
    layout->setColumnStretch(6, 1);
    return grid;
}

// The left column: Common and Memory and editing, always shown, then a header and the keys of every section.
QWidget* MainWindow::buildDirectKeys() {
    auto* keyboard = new QWidget;
    keyboard->setObjectName("directKeys");
    auto* layout = new QVBoxLayout(keyboard);
    const auto title = [keyboard](const char* name) {
        auto* label = new QLabel(keyboard);
        label->setObjectName(QString::fromLatin1(name));
        label->setForegroundRole(QPalette::PlaceholderText);
        return label;
    };
    QList<Key> common;
    for (const QString& id : defaultCommon()) common << directKey(id);
    layout->addWidget(title("commonTitle"));
    layout->addWidget(keyGrid("common", common, "common:"));
    layout->addWidget(title("memoryTitle"));
    layout->addWidget(keyGrid("memoryKeys", memoryKeys(), "direct:"));
    auto* rule = new QFrame(keyboard);
    rule->setObjectName("sectionsRule");
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Sunken);
    layout->addWidget(rule);
    // A section opens in place: its header (a caret and the title, then a dimmed preview of its keys while closed)
    // shows or hides its keys. Every section starts closed, and stays as left for the session.
    for (const KeySection& section : keySections()) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        auto* header = new QToolButton(keyboard);
        header->setObjectName("section:" + section.id);
        header->setAutoRaise(true);
        header->setFocusPolicy(Qt::NoFocus);
        header->setCheckable(true);
        header->setArrowType(Qt::RightArrow);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        QFont bold = header->font();
        bold.setBold(true);
        header->setFont(bold);
        auto* preview = new QLabel(keyboard);
        preview->setObjectName("preview:" + section.id);
        preview->setForegroundRole(QPalette::PlaceholderText);
        row->addWidget(header);
        row->addWidget(preview);
        row->addStretch();
        layout->addLayout(row);
        QWidget* keys = keyGrid("sectionKeys:" + section.id, section.keys, "direct:");
        keys->setVisible(false);
        layout->addWidget(keys);
        connect(header, &QToolButton::toggled, this, [header, preview, keys](bool open) {
            keys->setVisible(open);
            preview->setVisible(!open);
            header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        });
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
    all += everyDirectKey();
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
                             + 2 * keys_->frameWidth() + 2 * scrollBar,  // the window's bar and the column's
                         outer.top() + outer.bottom() + lcd_->minimumSizeHint().height() + 2 * keySpacing
                             + inner.top() + inner.bottom() + keypadGap
                             + style()->pixelMetric(QStyle::PM_TitleBarHeight));
    const QSize size = keySize(screen()->availableGeometry().size(), reserved, QSize(12, rows), keySpacing, minimum);
    const QSize numberSize((6 * size.width() + keySpacing) / 5, size.height());  // five span six

    for (const QList<Key>& row : keypad())
        for (const Key& key : row) {
            findChild<QPushButton*>("key:" + key.id)->setFixedSize(row.size() == 5 ? numberSize : size);
        }
    for (const Key& key : everyDirectKey()) findChild<QPushButton*>("direct:" + key.id)->setFixedSize(size);
    for (const QString& id : defaultCommon()) findChild<QPushButton*>("common:" + id)->setFixedSize(size);
    findChild<QWidget*>("cursorPad")->setFixedWidth(2 * size.width() + keySpacing);
    for (QWidget* w : {pad, direct}) {
        w->layout()->activate();
        w->adjustSize();
    }
    // Room for the column's scroll bar is kept, so the bar appearing moves nothing.
    directScroll_->setFixedWidth(direct->sizeHint().width() + scrollBar);
    directScroll_->setMinimumHeight(pad->sizeHint().height());
    area->layout()->activate();
    updatePreviews();
}

// While a section is closed, its header shows its keys' legends, as many as fit beside the title.
void MainWindow::updatePreviews() {
    const QWidget* common = findChild<QWidget*>("common");
    for (const KeySection& section : keySections()) {
        QStringList legends;
        for (const Key& key : section.keys) legends << translated(key.face.label);
        auto* preview = findChild<QLabel*>("preview:" + section.id);
        const QString text = legends.join(QStringLiteral("  "));
        if (!keysSized_) {
            preview->setText(text);
            continue;
        }
        const auto* header = findChild<QToolButton*>("section:" + section.id);
        const int room = common->sizeHint().width() - header->sizeHint().width() - keySpacing;
        preview->setText(preview->fontMetrics().elidedText(text, Qt::ElideRight, qMax(0, room)));
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (keysSized_) return;
    keysSized_ = true;
    sizeKeys();
    connect(windowHandle(), &QWindow::screenChanged, this, &MainWindow::sizeKeys);
}

// The gear: a small menu that opens from the button, with the values of the language and of the theme listed
// in place, then whether to calculate while typing. A choice applies at once and is forgotten at exit.
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
    addSetting(themeSection_, {"theme:system", "theme:light", "theme:dark"}, static_cast<int>(settings::theme()),
               [](int i) { settings::setTheme(static_cast<settings::Theme>(i)); });
    addSetting(decimalSection_, {"decimal:language", "decimal:point", "decimal:comma"}, static_cast<int>(settings::decimalSeparator()),
               [this](int i) {
                   settings::setDecimalSeparator(static_cast<settings::DecimalSeparator>(i));
                   if (hasResult_ || previewShown_) present();
                   relabelHistory();
                   lcd_->update();
               });
    inputSection_ = settings_->addSection(QString());
    QAction* live = settings_->addAction(QString());
    live->setObjectName("live");
    live->setCheckable(true);
    live->setChecked(settings::liveCalculation());
    connect(live, &QAction::toggled, this, [](bool on) { settings::setLiveCalculation(on); });
    // Nothing is remembered between runs; a file keeps the settings instead. The same dialogs serve the desktop
    // (native ones) and the browser (a download, and its file picker).
    fileSection_ = settings_->addSection(QString());
    QAction* exportAction = settings_->addAction(QString());
    exportAction->setObjectName("settings:export");
    connect(exportAction, &QAction::triggered, this,
            [this] { QFileDialog::saveFileContent(exportSettings(), QStringLiteral("calculate-settings.json"), this); });
    QAction* importAction = settings_->addAction(QString());
    importAction->setObjectName("settings:import");
    connect(importAction, &QAction::triggered, this, [this] {
        QFileDialog::getOpenFileContent(
            QStringLiteral("JSON (*.json)"),
            [this](const QString& name, const QByteArray& content) {
                if (name.isEmpty()) return;  // cancelled
                const QStringList problems = importSettings(content);
                if (!problems.isEmpty()) message_->setText(tr("Some settings were not imported: %1").arg(problems.join(QStringLiteral("; "))));
            },
            this);
    });
}

namespace {

// The file's names for the number types, in NumberType's order, and for the angle units, in the menu's.
const QStringList typeIds{"float", "double", "longdouble", "exact", "binary128", "binary256", "binary512"};
const QStringList angleIds{"rad", "deg", "grad"};

}  // namespace

QMap<QString, QStringList> MainWindow::settingKeys() const {
    QMap<QString, QStringList> keys;
    for (QAction* action : settings_->actions()) {
        if (!action->isCheckable()) continue;
        if (action->actionGroup()) keys[action->objectName().section(':', 0, 0)] << action->objectName().section(':', 1);
        else keys[action->objectName()] = QStringList{"true", "false"};
    }
    keys["type"] = typeIds;
    keys["angle"] = angleIds;
    return keys;
}

QMap<QString, QString> MainWindow::settingValues() const {
    QMap<QString, QString> values;
    for (QAction* action : settings_->actions()) {
        if (!action->isCheckable()) continue;
        if (!action->actionGroup()) values[action->objectName()] = action->isChecked() ? "true" : "false";
        else if (action->isChecked()) values[action->objectName().section(':', 0, 0)] = action->objectName().section(':', 1);
    }
    values["type"] = typeIds.value(static_cast<int>(type_->currentType()));
    values["angle"] = angleIds.value(angle_->currentIndex());
    return values;
}

QByteArray MainWindow::exportSettings() const { return settingsfile::write(settingValues(), defaults_); }

QStringList MainWindow::importSettings(const QByteArray& file) {
    const settingsfile::Read r = settingsfile::read(file, settingKeys());
    for (auto it = r.values.cbegin(); it != r.values.cend(); ++it) {
        if (it.key() == "type") {
            type_->setCurrentType(static_cast<NumberType>(typeIds.indexOf(it.value())));
        } else if (it.key() == "angle") {
            angle_->setCurrentIndex(static_cast<int>(angleIds.indexOf(it.value())));
        } else if (QAction* checkable = findChild<QAction*>(it.key())) {
            checkable->setChecked(it.value() == "true");
        } else {
            findChild<QAction*>(it.key() + ":" + it.value())->trigger();
        }
    }
    return r.problems;
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
    copyButton_->setText(tr("Copy"));
    copyButton_->setAccessibleName(copyButton_->text());
    copyAsMenu_->setTitle(tr("Copy as"));
    findChild<QAction*>("copy:value")->setText(tr("Value"));
    findChild<QAction*>("copy:trusted")->setText(tr("Trusted digits"));
    findChild<QAction*>("copy:bound")->setText(tr("Value ± bound"));
    findChild<QAction*>("copy:details")->setText(tr("Details as text"));
    findChild<QAction*>("copy:expression")->setText(tr("Expression"));
    findChild<QAction*>("history:copyExpression")->setText(tr("Copy expression"));
    findChild<QAction*>("history:copyValue")->setText(tr("Copy value"));
    findChild<QAction*>("history:copyBound")->setText(tr("Copy value ± bound"));
    editButton_->setText(tr("Edit"));
    editButton_->setAccessibleName(editButton_->text());
    keyboardButton_->setText(tr("Keyboard"));
    keyboardButton_->setAccessibleName(keyboardButton_->text());
    proceed_->setText(tr("Proceed anyway"));
    busy_->setText(tr("Computing…"));
    cancel_->setText(tr("Cancel"));
    historyToggle_->setToolTip(tr("History"));
    // The symbol buttons (☰, the gear, ▾) are spoken by their tooltips.
    for (QToolButton* button : {panelToggle_, settingsButton_, historyToggle_}) button->setAccessibleName(button->toolTip());
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
    decimalSection_->setText(tr("Decimal separator"));
    findChild<QAction*>("decimal:language")->setText(tr("As the language"));
    findChild<QAction*>("decimal:point")->setText(tr("Point"));
    findChild<QAction*>("decimal:comma")->setText(tr("Comma"));
    inputSection_->setText(tr("Input"));
    findChild<QAction*>("live")->setText(tr("Calculate as you type"));
    fileSection_->setText(tr("Settings file"));
    findChild<QAction*>("settings:export")->setText(tr("Export settings…"));
    findChild<QAction*>("settings:import")->setText(tr("Import settings…"));

    QList<Key> keys = cursorPad();
    for (const QList<Key>& row : keypad()) keys += row;
    for (const Key& key : keys) findChild<QPushButton*>("key:" + key.id)->setText(translated(key.face.label));
    findChild<QLabel*>("commonTitle")->setText(tr("Common"));
    findChild<QLabel*>("memoryTitle")->setText(tr("Memory and editing"));
    for (const KeySection& section : keySections())
        findChild<QToolButton*>("section:" + section.id)->setText(translated(section.title));
    for (const Key& key : everyDirectKey()) findChild<QPushButton*>("direct:" + key.id)->setText(translated(key.face.label));
    for (const QString& id : defaultCommon())
        findChild<QPushButton*>("common:" + id)->setText(translated(directKey(id).face.label));

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

    if (hasResult_ || previewShown_) present();
    relabelHistory();  // the decimal separator may follow the language
    updateKeys();
    updatePreviews();
    if (keysSized_) sizeKeys();  // labels changed width
}

// The icons are drawn in the text colour of the theme in use, so they are redrawn when it changes.
void MainWindow::drawIcons() {
    const QColor ink = palette().color(QPalette::ButtonText);
    settingsButton_->setIcon(icons::drawn(icons::Kind::Settings, ink, fontMetrics().height()));
    statisticsKeysToggle_->setIcon(icons::drawn(icons::Kind::Keyboard, ink, fontMetrics().height()));
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
    if (event->type() == QEvent::PaletteChange && settingsButton_) drawIcons();
    QMainWindow::changeEvent(event);
}

QWidget* MainWindow::buildStatistics() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* header = new QHBoxLayout;
    statisticsLabel_ = new QLabel(page);
    statisticsKeysToggle_ = new QToolButton(page);
    statisticsKeysToggle_->setObjectName("statisticsKeysToggle");
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
    for (int i = 0; i < statisticsKeys().size(); ++i) {
        const Key& key = statisticsKeys()[i];
        const QString f = key.face.function;
        auto* b = new QPushButton(key.face.label, page);
        b->setObjectName("stat:" + key.id);
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
    lcd_->finishName();  // the = key ends a name being typed, as Enter does
    keepsUnfinished_ = false;
    const QString text = lcd_->input().trimmed();
    if (text.isEmpty()) return;
    liveTimer_.stop();
    dropPreviews();
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
    previewShown_ = false;
    present();
    if (result.error) return;
    if (lcd_->input().trimmed() == expression) lcd_->setFresh(true);  // not when another input is being typed
    if (history_->count() == 0 || history_->item(0)->data(Qt::UserRole).toString() != expression) {
        QString value = lcd_->outputText();
        if (settings::decimalComma()) value.replace(',', '.');  // kept as the engine writes it, for relabelling
        auto* item = new QListWidgetItem(historyLabel(expression, value));
        item->setData(Qt::UserRole, expression);
        item->setData(Qt::UserRole + 1, value);
        history_->insertItem(0, item);
        Entry entry = typed_;
        if (entry.text().trimmed() != expression) entry.setRoot(typing::read(expression));  // not what was typed last
        historyEntries_.insert(historyEntries_.begin(), entry);
        historyResults_.insert(historyResults_.begin(), result);
        historyToggle_->setEnabled(true);
    }
    historyIndex_ = -1;
}

void MainWindow::requestPreview() {
    const QString text = lcd_->input().trimmed();
    dropPreviews();  // also when the input is now empty: an answer still on its way would show on a blank screen
    if (text.isEmpty()) {
        showNoPreview();
        return;
    }
    emit previewRequested(previewSerial_, text, options());
    previewLimit_.start();
}

void MainWindow::showPreview(int generation, const QString& expression, const Result& result) {
    if (generation != previewSerial_) return;
    previewLimit_.stop();
    preview_ = result;
    previewExpression_ = expression;
    previewShown_ = true;
    present();
}

// The worker skips a request whose generation is no longer the newest; the flag stops one already running.
// The generation goes first, so a request that starts between the two is skipped rather than run in full.
void MainWindow::dropPreviews() {
    worker_->previewGeneration() = ++previewSerial_;
    worker_->previewCancelFlag() = true;
    previewLimit_.stop();
}

// An unknown name that is the one being typed, and that some name completes: not wrong yet, only unfinished.
bool MainWindow::namePending(const Error& error, const QString& expression) const {
    if (error.code != ErrorCode::UnknownName) return false;
    const QString name = typing::nameBeingTyped(lcd_->entry());
    const QString part = QString::fromUtf8(expression.toUtf8().mid(static_cast<int>(error.begin), static_cast<int>(error.end - error.begin)));
    return !name.isEmpty() && part == name && !typing::completions(name).isEmpty();
}

// Nothing to show while typing: the result, the strip (but for the notice) and Details are blank.
void MainWindow::showNoPreview(const QString& notice) {
    previewShown_ = false;
    lcd_->setProvisional(false);
    lcd_->clearResult();
    detailsButton_->setEnabled(false);
    enableCopy(nullptr);
    message_->setText(notice);
    message_->setForegroundRole(QPalette::PlaceholderText);
}

void MainWindow::enableCopy(const Result* result) {
    copyButton_->setEnabled(result);
    copyAsMenu_->menuAction()->setEnabled(result);
    for (QAction* action : copyMenu_->actions()) action->setEnabled(result);
    QAction* trusted = findChild<QAction*>("copy:trusted");
    const bool none = result && view::copyText(*result, view::CopyForm::Trusted, types_[static_cast<std::size_t>(result->type)]).isEmpty();
    trusted->setEnabled(result && !none);
    trusted->setToolTip(none ? tr("No digit is trusted") : QString());
}

void MainWindow::popUpHistoryMenu(QPoint position) {
    QListWidgetItem* item = history_->itemAt(position);
    if (!item) return;
    history_->setCurrentItem(item);
    const QRect at(history_->viewport()->mapToGlobal(position), QSize(1, 1));
    historyMenu_->popup(placed(historyMenu_->sizeHint(), at, popupBounds(history_)).topLeft());
}

// "expression = value", the value cut short: the list only points back to the calculation.
QString MainWindow::historyLabel(const QString& expression, const QString& value) const {
    QString shown = shownExpression(value);
    if (shown.size() > 28) shown = shown.left(28) + QStringLiteral("…");
    return shownExpression(expression) + QStringLiteral(" = ") + shown;
}

void MainWindow::relabelHistory() {
    for (int i = 0; i < history_->count(); ++i) {
        QListWidgetItem* item = history_->item(i);
        item->setText(historyLabel(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toString()));
    }
}

QString MainWindow::shownExpression(const QString& expression) const {
    return settings::decimalComma() ? view::withDecimalComma(expression) : expression;
}

void MainWindow::showCompletions(const QString& name) {
    const QStringList names = typing::completions(name);
    if (name.size() < 2 || names.isEmpty() || names == QStringList{name}) {
        hideCompletions();
        return;
    }
    completions_->clear();
    completions_->addItems(names);
    completions_->setCurrentRow(0);
    const int frame = 2 * completions_->frameWidth();
    const QSize size(completions_->sizeHintForColumn(0) + frame + completions_->verticalScrollBar()->sizeHint().width(),
                     static_cast<int>(qMin(names.size(), completionRows)) * completions_->sizeHintForRow(0) + frame);
    QWidget* central = centralWidget();
    const QRect caret = lcd_->caretRectAt(lcd_->entry().position()).toAlignedRect();
    const QRect anchor(lcd_->mapTo(central, caret.topLeft()), caret.size());
    completions_->setGeometry(placed(size, anchor, central->rect()));
    completions_->raise();
    completions_->show();
    lcd_->setCompleting(true);
}

void MainWindow::hideCompletions() {
    completions_->hide();
    lcd_->setCompleting(false);
}

void MainWindow::chooseCompletion(const QString& name) {
    hideCompletions();
    lcd_->complete(name);
    lcd_->setFocus();
}

void MainWindow::popUpCopyMenu() {
    if (!copyButton_->isEnabled()) return;
    copyMenu_->popup(placed(copyMenu_->sizeHint(), globalGeometry(copyButton_), popupBounds(copyButton_)).topLeft());
}

// Shows the result being typed, or else the last one, on the screen and in the card, in the current language.
void MainWindow::present() {
    const Result& shown = shownResult();
    const QString& expression = previewShown_ ? previewExpression_ : lastExpression_;
    // While typing, an expression that only stops short is not a fault yet; the others are told, dimmed.
    const bool unfinished = previewShown_ && shown.error
                            && (view::incomplete(*shown.error) || lcd_->entry().hasEmptyBox() || namePending(*shown.error, expression));
    proceed_->setVisible(!previewShown_ && shown.error && canProceed(shown.error->code));  // it acts on the last request
    card_->setRows(view::details(shown, types_[static_cast<std::size_t>(shown.type)]));
    detailsButton_->setEnabled(!shown.error);
    enableCopy(shown.error ? nullptr : &shown);
    message_->setText(shown.error && !unfinished ? view::errorText(*shown.error, expression) : QString());
    message_->setForegroundRole(previewShown_ ? QPalette::PlaceholderText : QPalette::WindowText);
    lcd_->setProvisional(previewShown_);
    // The error's span is in bytes of the text that was evaluated: mark it only on that same input.
    const QString input = lcd_->input();
    if (shown.error && !unfinished && input.trimmed() == expression) {
        const int lead = static_cast<int>(input.left(input.indexOf(expression)).toUtf8().size());
        lcd_->setMarked(lead + static_cast<int>(shown.error->begin), lead + static_cast<int>(shown.error->end));
    } else {
        lcd_->clearMarked();
    }
    if (shown.error) lcd_->clearResult();
    else if (shown.exact) lcd_->showExact(settings::decimalComma() ? view::withDecimalComma(view::fractionParts(shown)) : view::fractionParts(shown));
    else lcd_->showValue(settings::decimalComma() ? view::withDecimalComma(view::valueParts(shown)) : view::valueParts(shown));
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
// Browsing past the newest row comes back to what was being typed before the browsing began.
void MainWindow::replay(int index) {
    if (index == -1 && keepsUnfinished_) {
        historyIndex_ = -1;
        keepsUnfinished_ = false;
        lcd_->setEntry(unfinished_);
        if (settings::liveCalculation()) liveTimer_.start();
        return;
    }
    if (index < 0 || index >= history_->count()) return;
    if (historyIndex_ == -1) {
        unfinished_ = lcd_->entry();
        keepsUnfinished_ = true;
    }
    historyIndex_ = index;
    lcd_->setEntry(historyEntries_[static_cast<std::size_t>(index)]);
    if (settings::liveCalculation()) liveTimer_.start();
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
    for (const Key& key : everyDirectKey()) keys.append({"direct:", key});
    for (const QString& id : defaultCommon()) keys.append({"common:", directKey(id)});
    for (const auto& [prefix, key] : keys) {
        auto* button = findChild<QPushButton*>(prefix + key.id);
        const bool on = available(key.face, exact);
        button->setEnabled(on);
        button->setToolTip(on ? QString() : exactRefusal(key.face.label));
    }
}
