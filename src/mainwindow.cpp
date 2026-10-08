#include "mainwindow.hpp"

#include "detailscard.hpp"
#include "formulatip.hpp"
#include "icons.hpp"
#include "keybutton.hpp"
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
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
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

// A value that may be longer than its place: cut short with "…" instead of widening the page; the whole text is its
// tooltip.
class ShortenedLabel : public QLabel {
public:
    using QLabel::QLabel;
    void setAnswer(const QString& text) {
        setText(text);
        setToolTip(text);
    }
    QSize minimumSizeHint() const override { return {fontMetrics().horizontalAdvance(QStringLiteral("0…")), QLabel::minimumSizeHint().height()}; }
    QSize sizeHint() const override { return minimumSizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideRight, width()));
    }
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), types_(numberTypes()) {
    common_ = defaultCommon();
    auto* central = new QWidget(this);
    auto* outer = new QHBoxLayout(central);

    // The left panel: ☰ collapses it to a thin rail; the gear (settings) stays at its bottom either way.
    rail_ = new QWidget(central);
    rail_->setObjectName("rail");
    railLayout_ = new QVBoxLayout(rail_);
    railLayout_->setContentsMargins(0, 0, 0, 0);
    panelToggle_ = new QToolButton(rail_);
    panelToggle_->setObjectName("panelToggle");
    panelToggle_->setText(QStringLiteral("☰"));
    panelToggle_->setCheckable(true);
    panelToggle_->setAutoRaise(true);
    modes_ = new QListWidget(rail_);
    modes_->setObjectName("modes");
    modes_->addItems({QString(), QString(), QString()});  // Calculator, Statistics, Percentages (see retranslate)
    modes_->setFixedWidth(140);
    settingsButton_ = new QToolButton(rail_);
    settingsButton_->setObjectName("settingsButton");
    settingsButton_->setAutoRaise(true);
    railLayout_->addWidget(panelToggle_, 0, Qt::AlignLeft);
    railLayout_->addWidget(modes_, 1);
    railLayout_->addStretch();  // keeps the gear at the bottom while the list is hidden
    railLayout_->addWidget(settingsButton_, 0, Qt::AlignLeft);
    outer->addWidget(rail_);
    connect(panelToggle_, &QToolButton::toggled, modes_, [this](bool collapsed) { modes_->setVisible(!collapsed); });
    // On a phone the rail is hidden and ☰ lists the modes in a small menu instead.
    modesMenu_ = new QMenu(this);
    modesMenu_->setObjectName("modesMenu");
    for (int i = 0; i < modes_->count(); ++i) {
        QAction* mode = modesMenu_->addAction(QString());  // the texts: see retranslate
        mode->setObjectName(QStringLiteral("mode:%1").arg(i));
        connect(mode, &QAction::triggered, this, [this, i] { modes_->setCurrentRow(i); });
    }
    connect(panelToggle_, &QToolButton::clicked, this, [this] {
        if (narrow_)
            modesMenu_->popup(placed(modesMenu_->sizeHint(), globalGeometry(panelToggle_), popupBounds(panelToggle_)).topLeft());
    });
    buildSettings();
    connect(settingsButton_, &QToolButton::clicked, this, [this] {
        settings_->popup(placed(settings_->sizeHint(), globalGeometry(settingsButton_), popupBounds(settingsButton_)).topLeft());
    });

    auto* main = new QVBoxLayout;
    outer->addLayout(main, 1);

    // The screen, and beside it the angle unit, the number type and = stacked to its height.
    screenRow_ = new QHBoxLayout;
    lcd_ = new Lcd(central);
    lcd_->setObjectName("lcd");
    screenRow_->addWidget(lcd_, 1);
    side_ = new QVBoxLayout;
    angle_ = new QComboBox(central);
    angle_->setObjectName("angle");
    // No AdjustToContents: angle, type and = get one fixed width for every language (see retranslate), and that policy
    // resizes a combo to its size hint 20 ms after its texts change, which leaves the side briefly out of its layout.
    angle_->addItems({QString(), QString(), QString()});  // RAD, DEG, GRAD (see retranslate)
    type_ = new TypeChooser(central);
    type_->setObjectName("type");
    equals_ = new QPushButton(central);
    equals_->setObjectName("equals");
    for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals_)}) {
        w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);  // as wide as the widest: see retranslate
        side_->addWidget(w);
    }
    screenRow_->addLayout(side_);
    main->addLayout(screenRow_);

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
                                                            {"copy:details", view::CopyForm::Details},
                                                            {"copy:concise", view::CopyForm::Concise}};
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
    // A key's other faces (a long press or a right click; see showMore).
    moreKeys_ = new QFrame(this, Qt::Popup);
    moreKeys_->setObjectName("moreKeys");
    moreKeys_->setFrameShape(QFrame::StyledPanel);
    auto* moreLayout = new QHBoxLayout(moreKeys_);
    moreLayout->setContentsMargins(keySpacing, keySpacing, keySpacing, keySpacing);
    moreLayout->setSpacing(keySpacing);
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
    keyboards_ = new QHBoxLayout(keysArea);
    keyboards_->setContentsMargins(0, 0, 0, 0);
    keyboards_->setSpacing(keypadGap);
    directScroll_ = new QScrollArea(keysArea);
    directScroll_->setObjectName("directScroll");
    directScroll_->setFrameShape(QFrame::NoFrame);
    directScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    directScroll_->setWidgetResizable(true);
    directScroll_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    directScroll_->setWidget(buildDirectKeys());
    QScroller::grabGesture(directScroll_->viewport(), QScroller::TouchGesture);  // a finger scrolls it (the phone's drawer)
    keyboards_->addStretch();
    keyboards_->addWidget(directScroll_);
    keyboards_->addWidget(buildKeypad(), 0, Qt::AlignTop);
    keyboards_->addStretch();
    // On a phone the rest of the column waits in a drawer over the main pad, opened from Common's last place.
    drawer_ = new QFrame(central);
    drawer_->setObjectName("drawer");
    drawer_->setFrameShape(QFrame::StyledPanel);
    drawer_->setAutoFillBackground(true);
    drawer_->hide();
    auto* drawerLayout = new QVBoxLayout(drawer_);
    drawerLayout->setContentsMargins(0, keySpacing, 0, keySpacing);  // the column's own margins are enough
    drawerClose_ = new QPushButton(drawer_);
    drawerClose_->setObjectName("drawerClose");
    drawerClose_->setFocusPolicy(Qt::NoFocus);
    drawerLayout->addWidget(drawerClose_);
    drawerToggle_ = new QPushButton(central);
    drawerToggle_->setObjectName("drawerToggle");
    drawerToggle_->setCheckable(true);
    drawerToggle_->setFocusPolicy(Qt::NoFocus);
    drawerToggle_->setMinimumWidth(32);
    drawerToggle_->hide();
    connect(drawerToggle_, &QPushButton::toggled, this, [this](bool open) {
        if (!open) {
            drawer_->hide();
            return;
        }
        QWidget* central = centralWidget();
        const int top = commonBlock_->mapTo(central, QPoint(0, commonBlock_->height())).y() + keySpacing;
        drawer_->setGeometry(0, top, central->width(), central->height() - top);
        drawer_->raise();
        drawer_->show();
    });
    connect(drawerClose_, &QPushButton::clicked, this, [this] { drawerToggle_->setChecked(false); });
    // What the search box finds: a list over the keys, not a window, so the keyboard stays with the box. An empty box
    // lists everything, so finding needs no keyboard; a finger scrolls it.
    searchList_ = new QListWidget(central);
    searchList_->setObjectName("searchList");
    searchList_->setFocusPolicy(Qt::NoFocus);
    searchList_->hide();
    QScroller::grabGesture(searchList_->viewport(), QScroller::TouchGesture);
    connect(search_, &QLineEdit::textEdited, this, &MainWindow::filterSearch);
    connect(keyboardButton_, &QToolButton::toggled, search_, [this](bool on) { search_->setAttribute(Qt::WA_InputMethodEnabled, on); });
    connect(searchList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const QVariant index = item->data(Qt::UserRole);
        if (!index.isValid()) return;  // a heading
        const Face face = searchEntries().value(index.toInt()).face;
        search_->clear();
        searchList_->hide();
        apply(face);
    });
    keys_->setWidget(keysArea);
    pages_->addWidget(keys_);
    auto* statistics = new QScrollArea(pages_);  // so neither page sets a minimum width for the window
    statistics->setFrameShape(QFrame::NoFrame);
    statistics->setWidgetResizable(true);
    statistics->setWidget(buildStatistics());
    pages_->addWidget(statistics);
    auto* percentages = new QScrollArea(pages_);
    percentages->setObjectName("percentagesScroll");
    percentages->setFrameShape(QFrame::NoFrame);
    percentages->setWidgetResizable(true);
    percentages->setWidget(buildPercentages());
    pages_->addWidget(percentages);
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
    connect(this, &MainWindow::memoryStoreRequested, worker_, &Worker::memoryStore);
    connect(this, &MainWindow::memoryClearRequested, worker_, &Worker::memoryClear);
    connect(this, &MainWindow::previewRequested, worker_, &Worker::preview);
    connect(worker_, &Worker::evaluated, this, &MainWindow::showResult);
    connect(worker_, &Worker::previewed, this, &MainWindow::showPreview);
    connect(this, &MainWindow::answerRequested, worker_, &Worker::answer);
    connect(worker_, &Worker::answered, this, &MainWindow::showPercentage);
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
    connect(lcd_, &Lcd::nameTyped, this, [this] {  // after every edit and movement: the call the cursor is in
        const typing::Call call = typing::callAround(lcd_->entry());
        lcd_->setHint(call.name.isEmpty() ? view::HintParts{} : view::argumentHint(call.name, call.argument));
    });
    connect(completions_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) { chooseCompletion(item->data(Qt::UserRole).toString()); });
    connect(lcd_, &Lcd::completionKey, this, [this](int key) {
        const int row = completions_->currentRow();
        if (key == Qt::Key_Up) completions_->setCurrentRow(qMax(row - 1, 0));
        else if (key == Qt::Key_Down) completions_->setCurrentRow(qMin(row + 1, completions_->count() - 1));
        else if (key == Qt::Key_Escape) hideCompletions();
        else chooseCompletion(completions_->currentItem()->data(Qt::UserRole).toString());  // Tab, Enter: choose, don't evaluate
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
    connect(type_, &QComboBox::currentIndexChanged, this, &MainWindow::reevaluate);
    connect(angle_, &QComboBox::currentIndexChanged, this, &MainWindow::reevaluate);
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
    auto* button = new KeyButton;
    button->setObjectName(prefix + key.id);
    button->setMinimumWidth(32);          // below the style's default, so every column can be equally wide
    button->setFocusPolicy(Qt::NoFocus);  // the keyboard always stays with the screen
    connect(button, &QPushButton::clicked, this, [this, key, prefix] {
        if (!(editingCommon_ && editCommon(key, prefix))) apply(key.face);
    });
    for (const auto& [id, others] : alternates())
        if (prefix == QLatin1String("key:") && id == key.id) {
            button->setHasMore(true);
            connect(button, &KeyButton::moreRequested, this, [this, button, others = others] { showMore(button, others); });
        }
    return button;
}

// A key's other faces in a small popup next to it; choosing one types it and closes the popup.
void MainWindow::showMore(QWidget* key, const QStringList& others) {
    for (QPushButton* old : moreKeys_->findChildren<QPushButton*>()) {
        old->setObjectName({});
        old->hide();
        old->deleteLater();
    }
    const QString id = key->objectName().section(':', 1);
    for (int i = 0; i < others.size(); ++i) {
        const Face face = directKey(others[i]).face;
        auto* button = new QPushButton(legend(face, settings::decimalComma()), moreKeys_);
        button->setObjectName(QStringLiteral("more:%1:%2").arg(id).arg(i));
        button->setFocusPolicy(Qt::NoFocus);
        button->setMinimumSize(key->size());
        moreKeys_->layout()->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, face] {
            moreKeys_->hide();
            apply(face);
        });
    }
    moreKeys_->adjustSize();
    moreKeys_->setGeometry(placed(moreKeys_->sizeHint(), globalGeometry(key), popupBounds(key)));
    moreKeys_->show();
}

// While Common is being edited, a click on one of its keys removes it, and a click on a section key adds it (at the
// end, while there is room); nothing is typed. Returns false for the keys that still type (Memory and editing).
bool MainWindow::editCommon(const Key& key, const QString& prefix) {
    if (prefix == QLatin1String("common:")) {
        QStringList ids = common_;
        ids.removeOne(key.id);
        setCommon(ids);
        return true;
    }
    const bool memory = std::any_of(memoryKeys().begin(), memoryKeys().end(), [&](const Key& k) { return k.id == key.id; });
    if (memory) return false;
    if (common_.contains(key.id)) return true;
    if (common_.size() >= commonLimit) {
        message_->setText(tr("Common holds at most %1 keys").arg(commonLimit));
        return true;
    }
    setCommon(common_ + QStringList{key.id});
    return true;
}

// Rebuilds Common's keys for `ids` (each a section key's id), sized, labelled and enabled like the others.
void MainWindow::setCommon(const QStringList& ids) {
    common_ = ids;
    QWidget* grid = findChild<QWidget*>("common");
    for (QPushButton* old : grid->findChildren<QPushButton*>(QRegularExpression(QStringLiteral("^common:")))) {
        grid->layout()->removeWidget(old);
        old->hide();
        old->setObjectName({});  // gone at once for findChild; deleted later (it may be the button being clicked)
        old->deleteLater();
    }
    for (const QString& id : common_) {
        const Key key = directKey(id);
        QPushButton* button = buildKey(key, "common:");
        button->setParent(grid);
        button->setText(legend(key.face, settings::decimalComma()));
        button->setAccessibleName(spokenName(key));
        if (keySize_.isValid()) {
            button->setFixedSize(keySize_);
            button->setFont(fittedFont(font(), button->text(), keySize_.width() - 10));
        }
    }
    placeCommon();
    updateKeys();
}

// Common's keys in rows of six; on a phone the twelfth place opens the drawer instead.
void MainWindow::placeCommon() {
    auto* layout = static_cast<QGridLayout*>(findChild<QWidget*>("common")->layout());
    for (int i = 0; i < common_.size(); ++i) {
        QPushButton* button = findChild<QPushButton*>("common:" + common_[i]);
        layout->addWidget(button, i / 6, i % 6);
        button->setVisible(!narrow_ || i < commonLimit - 1);
    }
    if (narrow_) {
        layout->addWidget(drawerToggle_, 1, 5);
        drawerToggle_->show();
    } else {
        layout->removeWidget(drawerToggle_);
        drawerToggle_->hide();
    }
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
    const auto title = [](const char* name) {
        auto* label = new QLabel;
        label->setObjectName(QString::fromLatin1(name));
        label->setForegroundRole(QPalette::PlaceholderText);
        return label;
    };
    QList<Key> common;
    for (const QString& id : common_) common << directKey(id);
    // The search box finds any function or constant; it takes the keyboard only when clicked.
    search_ = new QLineEdit(keyboard);
    search_->setObjectName("search");
    search_->setClearButtonEnabled(true);
    search_->setFocusPolicy(Qt::ClickFocus);
    search_->setAttribute(Qt::WA_InputMethodEnabled, lcd_->testAttribute(Qt::WA_InputMethodEnabled));
    search_->addAction(QIcon(), QLineEdit::LeadingPosition)->setObjectName("searchIcon");  // drawn: see drawIcons
    search_->installEventFilter(this);
    layout->addWidget(search_);
    // Common and its title move together: on a phone they sit above the main pad.
    commonBlock_ = new QWidget(keyboard);
    commonBlock_->setObjectName("commonBlock");
    auto* block = new QVBoxLayout(commonBlock_);
    block->setContentsMargins(0, 0, 0, 0);
    // Its title row: Edit lets the user change which keys Common holds; Reset (while editing) brings back the default.
    commonHeader_ = new QWidget(commonBlock_);
    commonHeader_->setObjectName("commonHeader");
    auto* header = new QHBoxLayout(commonHeader_);
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(title("commonTitle"));
    header->addStretch();
    editCommon_ = new QToolButton(commonHeader_);
    editCommon_->setObjectName("editCommon");
    editCommon_->setCheckable(true);
    editCommon_->setAutoRaise(true);
    editCommon_->setFocusPolicy(Qt::NoFocus);
    resetCommon_ = new QToolButton(commonHeader_);
    resetCommon_->setObjectName("resetCommon");
    resetCommon_->setAutoRaise(true);
    resetCommon_->setFocusPolicy(Qt::NoFocus);
    resetCommon_->hide();
    header->addWidget(resetCommon_);
    header->addWidget(editCommon_);
    connect(editCommon_, &QToolButton::toggled, this, [this](bool on) {
        editingCommon_ = on;
        resetCommon_->setVisible(on);
        message_->setText(on ? tr("Click a key to add it to Common, or a key of Common to remove it") : QString());
    });
    connect(resetCommon_, &QToolButton::clicked, this, [this] { setCommon(defaultCommon()); });
    block->addWidget(commonHeader_);
    block->addWidget(keyGrid("common", common, "common:"));
    layout->addWidget(commonBlock_);
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

// Lays the keys out for a screen of this size: side by side on a landscape screen, in the phone arrangement on a
// portrait one; then gives every key the size derived from the screen. Called when the window is first shown,
// whenever it moves to another screen, and after a change of language. What the window reserves for everything but
// the keys is measured from its own layouts, so it holds for any style, font and language.
void MainWindow::layOutKeys(QSize screen) {
    designScreen_ = screen;
    narrow_ = keysLayout(screen) == KeysLayout::Narrow;
    arrange();
    QWidget* area = keys_->widget();
    QWidget* pad = findChild<QWidget*>("keypad");
    QWidget* direct = findChild<QWidget*>("directKeys");
    QList<Key> all = memoryKeys();  // the keys always in view decide the size; longer legends shrink (fittedFont)
    for (const QList<Key>& row : keypad()) all += row;
    for (const QString& id : defaultCommon()) all << directKey(id);
    int labels = 0;  // the widest label in any language, so no key is too narrow for its name
    for (const Key& key : all)
        for (const QString& label : settings::inEveryLanguage("keypad", key.face.label))
            labels = qMax(labels, fontMetrics().horizontalAdvance(label));
    const QSize minimum(labels + 10, fontMetrics().height() + 10);

    const QMargins outer = centralWidget()->layout()->contentsMargins();
    const QMargins inner = pad->layout()->contentsMargins();
    const QMargins left = direct->layout()->contentsMargins();
    const int scrollBar = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    QSize size;
    if (narrow_) {
        // The phone's keys share the screen's width, and stay large enough for a finger. Six keys fit both under the
        // screen (beside the main pad's margins) and in the drawer (beside the column's margins and its scroll bar).
        const int besidePad = outer.left() + outer.right() + inner.left() + inner.right() + 2 * keys_->frameWidth();
        const int inDrawer = 2 * drawer_->frameWidth() + left.left() + left.right() + scrollBar;
        const int reservedWidth = qMax(besidePad, inDrawer);
        size = phoneKeySize(screen, reservedWidth, 6, keySpacing,
                            QSize(2 * fontMetrics().height(), qMax(touchTarget, minimum.height())));
    } else {
        const int rows = 7;
        const QSize reserved(outer.left() + outer.right() + centralWidget()->layout()->spacing() + modes_->width()
                                 + inner.left() + inner.right() + left.left() + left.right() + area->layout()->spacing()
                                 + 2 * keys_->frameWidth() + 2 * scrollBar,  // the window's bar and the column's
                             outer.top() + outer.bottom() + lcd_->minimumSizeHint().height() + 2 * keySpacing
                                 + inner.top() + inner.bottom() + keypadGap
                                 + style()->pixelMetric(QStyle::PM_TitleBarHeight));
        size = keySize(screen, reserved, QSize(12, rows), keySpacing, minimum);
    }
    const QSize numberSize((6 * size.width() + keySpacing) / 5, size.height());  // five span six

    for (const QList<Key>& row : keypad())
        for (const Key& key : row) {
            findChild<QPushButton*>("key:" + key.id)->setFixedSize(row.size() == 5 ? numberSize : size);
        }
    for (const Key& key : everyDirectKey()) findChild<QPushButton*>("direct:" + key.id)->setFixedSize(size);
    keySize_ = size;
    for (const QString& id : common_) findChild<QPushButton*>("common:" + id)->setFixedSize(size);
    drawerToggle_->setFixedSize(size);
    findChild<QWidget*>("cursorPad")->setFixedWidth(2 * size.width() + keySpacing);
    for (QPushButton* key : findChildren<QPushButton*>(QRegularExpression(QStringLiteral("^(key|direct|common):"))))
        key->setFont(fittedFont(font(), key->text(), key->width() - 10));
    drawerToggle_->setFont(fittedFont(font(), drawerToggle_->text(), size.width() - 10));
    updatePreviews();  // before the column is measured: a preview in full could widen it
    for (QWidget* w : {pad, direct}) {
        w->layout()->activate();
        w->adjustSize();
    }
    if (narrow_) {  // the column fills the drawer
        directScroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        directScroll_->setMinimumSize(0, 0);
        directScroll_->setMaximumWidth(QWIDGETSIZE_MAX);
        direct->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        // Room for the column's scroll bar is kept, and the keys never take it, so the bar appearing moves nothing.
        directScroll_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        directScroll_->setFixedWidth(direct->sizeHint().width() + scrollBar);
        direct->setMaximumWidth(direct->sizeHint().width());
        directScroll_->setMinimumHeight(pad->sizeHint().height());
    }
    area->layout()->activate();
}

// Moves the widgets into the arrangement layOutKeys chose. Wide: the rail, the screen with angle, type and = beside
// it, and the column beside the main pad. Narrow (a phone): no rail; the screen at the full width with ☰, angle,
// type, = and the gear in a row under it; Common (its last place opening the drawer) above the main pad, at the
// bottom; the rest of the column in the drawer.
void MainWindow::arrange() {
    // Only a change of arrangement moves anything: relayouts at every call (a language change lays the keys out again)
    // would leave the window briefly unsettled.
    if (arranged_ == static_cast<int>(narrow_)) return;
    arranged_ = static_cast<int>(narrow_);
    QWidget* pad = findChild<QWidget*>("keypad");
    auto* column = static_cast<QVBoxLayout*>(findChild<QWidget*>("directKeys")->layout());
    auto* drawerLayout = static_cast<QVBoxLayout*>(drawer_->layout());
    const QList<QWidget*> side{angle_, type_, equals_};
    while (QLayoutItem* item = keyboards_->takeAt(0)) delete item;  // the widgets stay; the stretches go
    if (narrow_) {
        rail_->hide();
        panelToggle_->setChecked(false);
        panelToggle_->setCheckable(false);
        if (side_->indexOf(panelToggle_) < 0) {
            railLayout_->removeWidget(panelToggle_);
            railLayout_->removeWidget(settingsButton_);
            side_->insertWidget(0, panelToggle_);
            side_->addWidget(settingsButton_);
        }
        screenRow_->setDirection(QBoxLayout::TopToBottom);
        side_->setDirection(QBoxLayout::LeftToRight);
        for (QWidget* w : side) {
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            w->setMinimumWidth(0);
            w->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        for (QWidget* w : side + QList<QWidget*>{panelToggle_, settingsButton_}) w->setFixedHeight(touchTarget);
        column->removeWidget(commonBlock_);
        keyboards_->setDirection(QBoxLayout::TopToBottom);
        keyboards_->addStretch();
        keyboards_->addWidget(commonBlock_, 0, Qt::AlignHCenter);
        keyboards_->addWidget(pad, 0, Qt::AlignHCenter);
        if (drawerLayout->indexOf(directScroll_) < 0) drawerLayout->insertWidget(0, directScroll_);
        if (drawerLayout->indexOf(commonHeader_) < 0) drawerLayout->insertWidget(0, commonHeader_);  // Edit, in the drawer
        const QMargins column = findChild<QWidget*>("directKeys")->layout()->contentsMargins();
        commonHeader_->layout()->setContentsMargins(column.left(), 0, column.right(), 0);  // in line with the column
    } else {
        drawerToggle_->setChecked(false);
        rail_->show();
        panelToggle_->setCheckable(true);
        modes_->show();
        if (railLayout_->indexOf(panelToggle_) < 0) {
            side_->removeWidget(panelToggle_);
            side_->removeWidget(settingsButton_);
            railLayout_->insertWidget(0, panelToggle_, 0, Qt::AlignLeft);
            railLayout_->addWidget(settingsButton_, 0, Qt::AlignLeft);
        }
        for (QWidget* w : QList<QWidget*>{panelToggle_, settingsButton_}) {
            w->setMinimumHeight(0);
            w->setMaximumHeight(QWIDGETSIZE_MAX);
        }
        screenRow_->setDirection(QBoxLayout::LeftToRight);
        side_->setDirection(QBoxLayout::TopToBottom);
        for (QWidget* w : side) {
            w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);  // as wide as the widest: see retranslate
            w->setMinimumHeight(0);
            w->setMaximumHeight(QWIDGETSIZE_MAX);
            if (sideWidth_ > 0) w->setFixedWidth(sideWidth_);
        }
        if (drawerLayout->indexOf(commonHeader_) >= 0) {
            drawerLayout->removeWidget(commonHeader_);
            static_cast<QVBoxLayout*>(commonBlock_->layout())->insertWidget(0, commonHeader_);
            commonHeader_->layout()->setContentsMargins(0, 0, 0, 0);
        }
        if (column->indexOf(commonBlock_) < 0) column->insertWidget(1, commonBlock_);  // under the search box
        drawerLayout->removeWidget(directScroll_);
        keyboards_->setDirection(QBoxLayout::LeftToRight);
        keyboards_->addStretch();
        keyboards_->addWidget(directScroll_);
        keyboards_->addWidget(pad, 0, Qt::AlignTop);
        keyboards_->addStretch();
    }
    placeCommon();
}

void MainWindow::relabelLetters() {
    for (char c = 'a'; c <= 'z'; ++c) {
        const QString letter(QChar::fromLatin1(c));
        findChild<QPushButton*>("direct:letter" + letter.toUpper())->setText(shifted_ ? letter.toUpper() : letter);
    }
}

// The search list: every entry under its heading, in the language in use.
void MainWindow::fillSearch() {
    searchList_->clear();
    const QList<SearchEntry> entries = searchEntries();
    QString group;
    for (int i = 0; i < entries.size(); ++i) {
        const SearchEntry& e = entries[i];
        if (i == 0 || e.group != group) {
            group = e.group;
            auto* heading = new QListWidgetItem(translated(group), searchList_);
            heading->setFlags(Qt::ItemIsEnabled);
            QFont bold = heading->font();
            bold.setBold(true);
            heading->setFont(bold);
        }
        auto* item = new QListWidgetItem(legend(e.face, settings::decimalComma()) + (e.title.isEmpty() ? QString() : QStringLiteral("  —  ") + e.title),
                                         searchList_);
        item->setData(Qt::UserRole, i);
    }
    if (searchList_->isVisible()) filterSearch(search_->text());
}

// Hides the entries the text doesn't match, and the headings left without entries.
void MainWindow::filterSearch(const QString& text) {
    const QList<SearchEntry> entries = searchEntries();
    QListWidgetItem* heading = nullptr;
    bool any = false;
    for (int row = 0; row < searchList_->count(); ++row) {
        QListWidgetItem* item = searchList_->item(row);
        const QVariant index = item->data(Qt::UserRole);
        if (!index.isValid()) {
            if (heading) heading->setHidden(!any);
            heading = item;
            any = false;
            continue;
        }
        const bool match = searchMatches(entries.value(index.toInt()), text);
        item->setHidden(!match);
        any = any || match;
    }
    if (heading) heading->setHidden(!any);
    if (search_->hasFocus()) showSearch();
}

// Opens the list under the box: as wide as it, as tall as the rows it shows (twelve at most, never past the window's
// bottom), and hidden while nothing matches.
void MainWindow::showSearch() {
    int shown = 0;
    for (int row = 0; row < searchList_->count(); ++row) shown += searchList_->item(row)->isHidden() ? 0 : 1;
    if (shown == 0) {
        searchList_->hide();
        return;
    }
    QWidget* central = centralWidget();
    const QPoint top = search_->mapTo(central, QPoint(0, search_->height() + 2));
    const int rows = qMin(shown, 12) * qMax(1, searchList_->sizeHintForRow(0)) + 2 * searchList_->frameWidth();
    searchList_->setGeometry(top.x(), top.y(), search_->width(), qMax(0, qMin(rows, central->height() - top.y() - 2)));
    searchList_->raise();
    searchList_->show();
}

// While a section is closed, its header shows its keys' legends, as many as fit beside the title.
void MainWindow::updatePreviews() {
    const QWidget* common = findChild<QWidget*>("common");
    for (const KeySection& section : keySections()) {
        QStringList legends;
        for (const Key& key : section.keys) legends << legend(key.face, settings::decimalComma());
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
    layOutKeys(screen()->availableGeometry().size());
    connect(windowHandle(), &QWindow::screenChanged, this,
            [this](QScreen* screen) { layOutKeys(screen->availableGeometry().size()); });
}

// The gear: a small menu that opens from the button, with the values of the language and of the theme listed
// in place, then whether to calculate while typing. A choice applies at once and is forgotten at exit.
void MainWindow::buildSettings() {
    settings_ = new QMenu(this);
    settings_->setObjectName("settings");
    // One checkable action per value, in the order of the setting's enum; addSetting puts a heading first.
    const auto addGroup = [this](std::initializer_list<const char*> values, int checked, auto apply, QMenu* menu = nullptr) {
        if (!menu) menu = settings_;
        auto* group = new QActionGroup(menu);
        int i = 0;
        for (const char* value : values) {
            QAction* action = menu->addAction(QString());  // the texts: see retranslate
            action->setObjectName(QString::fromLatin1(value));
            action->setCheckable(true);
            action->setChecked(i == checked);
            group->addAction(action);
            connect(action, &QAction::triggered, this, [apply, i] { apply(i); });
            ++i;
        }
    };
    const auto addSetting = [this, addGroup](QAction*& section, std::initializer_list<const char*> values, int checked, auto apply) {
        section = settings_->addSection(QString());
        addGroup(values, checked, apply);
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
                   retranslate();  // the separator keys' legends, and the percentages again
               });
    // What log, mod and % mean. Earlier results keep theirs (the engine stores them spelled out); what is being
    // typed is worked out again.
    settings_->addSeparator();
    conventions_ = settings_->addMenu(QString());
    conventions_->setObjectName("conventions");
    const auto changeConvention = [this](auto change) {
        calculate_core::Conventions c = settings::conventions();
        change(c);
        settings::setConventions(c);
        updateKeys();  // the log key's tooltip
        if (settings::liveCalculation() && (previewShown_ || lcd_->input().trimmed() != lastExpression_)) requestPreview();
    };
    using calculate_core::Conventions;
    addGroup({"log:10", "log:e"}, static_cast<int>(settings::conventions().log),
             [changeConvention](int i) { changeConvention([i](Conventions& c) { c.log = static_cast<Conventions::Log>(i); }); }, conventions_);
    conventions_->addSeparator();
    addGroup({"mod:truncated", "mod:floored"}, static_cast<int>(settings::conventions().mod),
             [changeConvention](int i) { changeConvention([i](Conventions& c) { c.mod = static_cast<Conventions::Mod>(i); }); }, conventions_);
    conventions_->addSeparator();
    addGroup({"percent:divide", "percent:ofvalue"}, static_cast<int>(settings::conventions().percent),
             [changeConvention](int i) { changeConvention([i](Conventions& c) { c.percent = static_cast<Conventions::Percent>(i); }); }, conventions_);
    // How uncertain inputs combine, and whether typed numbers carry half a unit of their last digit.
    uncertainty_ = settings_->addMenu(QString());
    uncertainty_->setObjectName("uncertaintySettings");
    uncertaintySection_ = uncertainty_->addSection(QString());
    addGroup({"uncertainty:worst", "uncertainty:statistical"}, 0, [this](int i) {
        rule_ = static_cast<calculate_core::UncertaintyRule>(i);
        reevaluate();
    }, uncertainty_);
    readingSection_ = uncertainty_->addSection(QString());
    addGroup({"readprecision:off", "readprecision:decimals", "readprecision:all"}, 0, [this](int i) {
        reading_ = static_cast<calculate_core::ReadPrecision>(i);
        reevaluate();
    }, uncertainty_);
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

QList<QAction*> MainWindow::settingActions() const {
    return settings_->actions() + conventions_->actions() + uncertainty_->actions();
}

QMap<QString, QStringList> MainWindow::settingKeys() const {
    QMap<QString, QStringList> keys;
    for (QAction* action : settingActions()) {
        if (!action->isCheckable()) continue;
        if (action->actionGroup()) keys[action->objectName().section(':', 0, 0)] << action->objectName().section(':', 1);
        else keys[action->objectName()] = QStringList{"true", "false"};
    }
    keys["type"] = typeIds;
    keys["angle"] = angleIds;
    for (const KeySection& section : keySections())
        for (const Key& key : section.keys) keys["common"] << key.id;  // a list of them (see importSettings)
    return keys;
}

QMap<QString, QString> MainWindow::settingValues() const {
    QMap<QString, QString> values;
    for (QAction* action : settingActions()) {
        if (!action->isCheckable()) continue;
        if (!action->actionGroup()) values[action->objectName()] = action->isChecked() ? "true" : "false";
        else if (action->isChecked()) values[action->objectName().section(':', 0, 0)] = action->objectName().section(':', 1);
    }
    values["type"] = typeIds.value(static_cast<int>(type_->currentType()));
    values["angle"] = angleIds.value(angle_->currentIndex());
    values["common"] = common_.join(' ');
    return values;
}

QByteArray MainWindow::exportSettings() const { return settingsfile::write(settingValues(), defaults_); }

QStringList MainWindow::importSettings(const QByteArray& file) {
    settingsfile::Read r = settingsfile::read(file, settingKeys(), {QStringLiteral("common")});
    for (auto it = r.values.cbegin(); it != r.values.cend(); ++it) {
        if (it.key() == "common") {
            const QStringList ids = it.value().split(' ', Qt::SkipEmptyParts);
            if (canBeCommon(ids)) setCommon(ids);
            else r.problems << tr("“%1” cannot be the common keys").arg(it.value());
        } else if (it.key() == "type") {
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
    const QStringList modes{tr("Calculator"), tr("Statistics"), tr("Percentages")};
    for (int i = 0; i < modes.size(); ++i) modes_->item(i)->setText(modes[i]);
    for (int i = 0; i < modes_->count(); ++i) findChild<QAction*>(QStringLiteral("mode:%1").arg(i))->setText(modes_->item(i)->text());
    drawerToggle_->setText(tr("More") + QStringLiteral(" ▾"));
    drawerToggle_->setAccessibleName(tr("More"));
    search_->setPlaceholderText(tr("Search every function and constant…"));
    search_->setAccessibleName(tr("Search"));
    fillSearch();
    drawerClose_->setText(QStringLiteral("▾  ") + tr("Back to the keypad"));
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
    findChild<QAction*>("copy:concise")->setText(tr("Concise (1.23(4))"));
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
    percentKeysToggle_->setToolTip(tr("Show or hide the keypad"));
    findChild<QPushButton*>("percentKey:point")->setText(settings::decimalComma() ? QStringLiteral(",") : QStringLiteral("."));
    for (const view::PercentageRow& row : view::percentageRows(QStringLiteral("1"), QStringLiteral("2")))
        findChild<QLabel*>("percentTitle:" + row.key)->setText(row.title);
    requestPercentages();  // error texts are in the language too
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
    conventions_->setTitle(tr("Conventions"));
    findChild<QAction*>("log:10")->setText(tr("log is base 10"));
    findChild<QAction*>("log:e")->setText(tr("log is natural"));
    findChild<QAction*>("mod:truncated")->setText(tr("mod keeps the dividend's sign"));
    findChild<QAction*>("mod:floored")->setText(tr("mod keeps the divisor's sign"));
    findChild<QAction*>("percent:divide")->setText(tr("% divides by 100"));
    findChild<QAction*>("percent:ofvalue")->setText(tr("x + p% adds p% of x"));
    findChild<QAction*>("decimal:comma")->setText(tr("Comma"));
    uncertainty_->setTitle(tr("Uncertainty"));
    uncertaintySection_->setText(tr("Combination"));
    findChild<QAction*>("uncertainty:worst")->setText(tr("Worst case (a limit)"));
    findChild<QAction*>("uncertainty:statistical")->setText(tr("Statistical (an estimate)"));
    readingSection_->setText(tr("Read precision"));
    findChild<QAction*>("readprecision:off")->setText(tr("Off"));
    findChild<QAction*>("readprecision:decimals")->setText(tr("Decimals"));
    findChild<QAction*>("readprecision:all")->setText(tr("All numbers"));
    inputSection_->setText(tr("Input"));
    findChild<QAction*>("live")->setText(tr("Calculate as you type"));
    fileSection_->setText(tr("Settings file"));
    findChild<QAction*>("settings:export")->setText(tr("Export settings…"));
    findChild<QAction*>("settings:import")->setText(tr("Import settings…"));

    QList<Key> keys = cursorPad();
    for (const QList<Key>& row : keypad()) keys += row;
    for (const Key& key : keys) {
        auto* button = findChild<QPushButton*>("key:" + key.id);
        button->setText(legend(key.face, settings::decimalComma()));
        button->setAccessibleName(spokenName(key));
    }
    findChild<QLabel*>("commonTitle")->setText(tr("Common"));
    findChild<QLabel*>("memoryTitle")->setText(tr("Memory and editing"));
    for (const KeySection& section : keySections()) {
        auto* header = findChild<QToolButton*>("section:" + section.id);
        header->setText(translated(section.title));
        header->setAccessibleName(header->text());
    }
    for (const Key& key : everyDirectKey()) {
        auto* button = findChild<QPushButton*>("direct:" + key.id);
        button->setText(legend(key.face, settings::decimalComma()));
        button->setAccessibleName(spokenName(key));
    }
    editCommon_->setText(tr("Edit"));
    resetCommon_->setText(tr("Reset"));
    for (const QString& id : common_) {
        auto* button = findChild<QPushButton*>("common:" + id);
        button->setText(legend(directKey(id).face, settings::decimalComma()));
        button->setAccessibleName(spokenName(directKey(id)));
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
    sideWidth_ = width;
    if (!narrow_)
        for (QWidget* w : {static_cast<QWidget*>(angle_), static_cast<QWidget*>(type_), static_cast<QWidget*>(equals_)})
            w->setFixedWidth(width);

    if (hasResult_ || previewShown_) present();
    relabelHistory();  // the decimal separator may follow the language
    updateKeys();
    updatePreviews();
    if (keysSized_) layOutKeys(designScreen_);  // labels changed width
}

// The icons are drawn in the text colour of the theme in use, so they are redrawn when it changes.
void MainWindow::drawIcons() {
    const QColor ink = palette().color(QPalette::ButtonText);
    settingsButton_->setIcon(icons::drawn(icons::Kind::Settings, ink, fontMetrics().height()));
    search_->findChild<QAction*>("searchIcon")->setIcon(
        icons::drawn(icons::Kind::Search, palette().color(QPalette::PlaceholderText), fontMetrics().height()));
    statisticsKeysToggle_->setIcon(icons::drawn(icons::Kind::Keyboard, ink, fontMetrics().height()));
    percentKeysToggle_->setIcon(icons::drawn(icons::Kind::Keyboard, ink, fontMetrics().height()));
}

// Hovering a statistics button shows how that statistic is computed.
bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == percentFirst_ || watched == percentSecond_) && event->type() == QEvent::FocusIn)
        percentTarget_ = static_cast<QLineEdit*>(watched);
    if (watched == search_) {
        if (event->type() == QEvent::FocusIn) filterSearch(search_->text());
        if (event->type() == QEvent::FocusOut) searchList_->hide();
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            search_->clear();
            searchList_->hide();
            lcd_->setFocus();
            return true;
        }
    }
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
    if (event->type() == QEvent::PaletteChange && hasResult_) present();  // the bits' colours follow the theme
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

// Two values and the percentage questions about them, each answered with its bound.
QWidget* MainWindow::buildPercentages() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* entry = new QGridLayout;
    // Only what a number is made of, typed or pasted (as in the statistics box).
    auto numeric = [this](QLineEdit* edit) {
        connect(edit, &QLineEdit::textChanged, edit, [edit](const QString& text) {
            QString kept = text;
            kept.remove(QRegularExpression(QStringLiteral("[^0-9.,eE-]")));
            if (kept != text) edit->setText(kept);
        });
        connect(edit, &QLineEdit::textChanged, this, [this] { percentTimer_.start(); });
        edit->installEventFilter(this);  // a box that takes the focus becomes the keypad's (see eventFilter)
    };
    percentFirst_ = new QLineEdit(page);
    percentFirst_->setObjectName("percentFirst");
    percentSecond_ = new QLineEdit(page);
    percentSecond_->setObjectName("percentSecond");
    percentTarget_ = percentFirst_;
    percentKeysToggle_ = new QToolButton(page);
    percentKeysToggle_->setObjectName("percentKeysToggle");
    percentKeysToggle_->setCheckable(true);
    percentKeysToggle_->setChecked(true);
    percentKeysToggle_->setAutoRaise(true);
    percentKeysToggle_->setFocusPolicy(Qt::NoFocus);
    entry->addWidget(new QLabel(QStringLiteral("1"), page), 0, 0);
    entry->addWidget(percentFirst_, 0, 1);
    entry->addWidget(percentKeysToggle_, 0, 2);
    entry->addWidget(new QLabel(QStringLiteral("2"), page), 1, 0);
    entry->addWidget(percentSecond_, 1, 1);
    numeric(percentFirst_);
    numeric(percentSecond_);
    layout->addLayout(entry);

    // A small numeric keypad, so a phone needs no system keyboard; it types into the box being edited.
    auto* keys = new QWidget(page);
    keys->setObjectName("percentKeys");
    auto* grid = new QGridLayout(keys);
    grid->setContentsMargins(0, 0, 0, 0);
    const struct {
        const char* id;
        const char* label;
        const char* insert;  // empty: delete the character before the cursor
    } pad[] = {{"7", "7", "7"}, {"8", "8", "8"}, {"9", "9", "9"}, {"backspace", "⌫", ""},
               {"4", "4", "4"}, {"5", "5", "5"}, {"6", "6", "6"}, {"next", "⏎", "\n"},
               {"1", "1", "1"}, {"2", "2", "2"}, {"3", "3", "3"}, {"minus", "−", "-"},
               {"0", "0", "0"}, {"point", ".", "."}};
    for (int i = 0; i < int(std::size(pad)); ++i) {
        auto* button = new QPushButton(QString::fromUtf8(pad[i].label), keys);
        button->setObjectName(QStringLiteral("percentKey:") + pad[i].id);
        button->setFocusPolicy(Qt::NoFocus);
        const QString insert = QString::fromUtf8(pad[i].insert);
        connect(button, &QPushButton::clicked, this, [this, insert] {
            // The keypad keeps its own target: a browser may not give the page the keyboard focus.
            QLineEdit* edit = percentTarget_;
            if (insert == "\n") {
                percentTarget_ = edit == percentFirst_ ? percentSecond_ : percentFirst_;
                percentTarget_->setFocus();
            } else if (insert.isEmpty()) edit->backspace();
            else edit->insert(settings::decimalComma() && insert == "." ? QStringLiteral(",") : insert);
        });
        grid->addWidget(button, i / 4, i % 4);
    }
    connect(percentKeysToggle_, &QToolButton::toggled, keys, &QWidget::setVisible);
    layout->addWidget(keys, 0, Qt::AlignLeft);

    auto* answers = new QGridLayout;
    const QList<view::PercentageRow> rows = view::percentageRows(QStringLiteral("1"), QStringLiteral("2"));
    for (int i = 0; i < rows.size(); ++i) {
        auto* title = new QLabel(page);  // see retranslate
        title->setObjectName("percentTitle:" + rows[i].key);
        auto* value = new ShortenedLabel(page);
        value->setObjectName("percent:" + rows[i].key);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* bound = new QLabel(page);
        bound->setObjectName("percentBound:" + rows[i].key);
        bound->setForegroundRole(QPalette::PlaceholderText);
        bound->setWordWrap(true);
        answers->addWidget(title, i, 0);
        answers->addWidget(value, i, 1);
        answers->addWidget(bound, i, 2);
    }
    answers->setColumnStretch(1, 1);
    layout->addLayout(answers);
    layout->addStretch();
    percentTimer_.setSingleShot(true);
    percentTimer_.setInterval(liveDelay);
    connect(&percentTimer_, &QTimer::timeout, this, [this] {
        // Answers still on their way are skipped or cancelled, as for the screen's live result.
        worker_->answerGeneration() = ++percentSerial_;
        worker_->answerCancelFlag() = true;
        QString first = percentFirst_->text(), second = percentSecond_->text();
        if (settings::decimalComma()) {  // the engine reads a point
            first.replace(',', '.');
            second.replace(',', '.');
        }
        const QList<view::PercentageRow> rows = view::percentageRows(first, second);
        for (const view::PercentageRow& row : view::percentageRows(QStringLiteral("1"), QStringLiteral("2"))) {
            static_cast<ShortenedLabel*>(findChild<QLabel*>("percent:" + row.key))->setAnswer({});
            findChild<QLabel*>("percentBound:" + row.key)->clear();
        }
        for (const view::PercentageRow& row : rows) emit answerRequested(percentSerial_, row.key, row.expression, options());
    });
    return page;
}

void MainWindow::requestPercentages() {
    if (percentFirst_) percentTimer_.start();
}

void MainWindow::showPercentage(int generation, const QString& key, const Result& result) {
    if (generation != percentSerial_) return;
    auto* value = static_cast<ShortenedLabel*>(findChild<QLabel*>("percent:" + key));
    auto* bound = findChild<QLabel*>("percentBound:" + key);
    if (result.error) {
        value->setAnswer({});
        bound->setText(view::errorText(*result.error, QString::fromStdString(result.expression)));
        return;
    }
    const bool comma = settings::decimalComma();
    if (result.exact) {
        value->setAnswer(view::oneLine(comma ? view::withDecimalComma(view::fractionParts(result)) : view::fractionParts(result)));
        bound->setText(QStringLiteral("± 0"));
        return;
    }
    value->setAnswer(view::oneLine(comma ? view::withDecimalComma(view::valueParts(result)) : view::valueParts(result)));
    const QString text = QStringLiteral("± ") + QString::fromStdString(result.bound);
    bound->setText(comma ? view::withDecimalComma(text) : text);
}

Options MainWindow::options() const {
    Options o;
    o.type = type_->currentType();
    o.angle = static_cast<AngleUnit>(angle_->currentIndex());
    o.conventions = settings::conventions();
    o.uncertaintyRule = rule_;
    o.readPrecision = reading_;
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
// `#` can only start a comment, so the text before the first one is the expression: "1+1 = 2   # two". A note (only a
// comment) is shown as typed.
QString MainWindow::historyLabel(const QString& expression, const QString& value) const {
    const QString typed = expression.section('#', 0, 0).trimmed();
    if (typed.isEmpty()) return expression.trimmed();
    QString shown = shownExpression(value);
    if (shown.size() > 28) shown = shown.left(28) + QStringLiteral("…");
    QString label = shownExpression(typed) + QStringLiteral(" = ") + shown;
    const QString comment = expression.section('#', 1).trimmed();
    if (!comment.isEmpty()) label += QStringLiteral("   # ") + comment;
    return label;
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
    for (const QString& completion : names) {  // "asin — Inverse sine"; the item keeps the name
        const QString title = typing::completionTitle(completion);
        auto* item = new QListWidgetItem(title.isEmpty() ? completion : completion + QStringLiteral(" — ") + title, completions_);
        item->setData(Qt::UserRole, completion);
    }
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
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    card_->setRows(view::details(shown, types_[static_cast<std::size_t>(shown.type)]) + view::storedRows(shown, view::bitColours(dark)));
    const bool valueless = shown.error || shown.commentOnly;  // an error, or a note
    detailsButton_->setEnabled(!valueless);
    enableCopy(valueless ? nullptr : &shown);
    // A note about a result that is not an error shows dimmed, like a live error.
    const QString offBy = shown.error ? QString() : view::offBy(shown);
    const bool note = !shown.error && (!shown.warnings.empty() || !offBy.isEmpty());
    message_->setText(shown.error && !unfinished ? view::errorText(*shown.error, expression)
                      : !shown.error && !shown.warnings.empty() ? view::warningText(shown.warnings.front(), expression)
                      : !offBy.isEmpty()                         ? QCoreApplication::translate("view", "≈: off by %1").arg(offBy)
                                                                 : QString());
    message_->setForegroundRole(previewShown_ || note ? QPalette::PlaceholderText : QPalette::WindowText);
    message_->setProperty("dimmed", previewShown_ || note);
    lcd_->setProvisional(previewShown_);
    // The error's span is in bytes of the text that was evaluated: mark it only on that same input.
    const QString input = lcd_->input();
    if (shown.error && !unfinished && input.trimmed() == expression) {
        const int lead = static_cast<int>(input.left(input.indexOf(expression)).toUtf8().size());
        lcd_->setMarked(lead + static_cast<int>(shown.error->begin), lead + static_cast<int>(shown.error->end));
    } else {
        lcd_->clearMarked();
    }
    // How the input was read, unless it adds nothing to what was typed.
    const QString reading = valueless ? QString() : QString::fromStdString(shown.reading);
    QString typed = input;
    typed.remove(' ');
    QString read = reading;
    read.remove(' ');
    lcd_->setReading(read == typed ? QString() : settings::decimalComma() ? view::withDecimalComma(reading) : reading);
    if (valueless) lcd_->clearResult();
    else if (const auto parts = view::conversionParts(shown))  // a number: drawn like a value, with its bar
        lcd_->showValue(settings::decimalComma() ? view::withDecimalComma(*parts) : *parts);
    else if (shown.conversion) lcd_->showText(view::conversionText(shown));
    else if (shown.exact) lcd_->showExact(settings::decimalComma() ? view::withDecimalComma(view::fractionParts(shown)) : view::fractionParts(shown));
    else lcd_->showValue(settings::decimalComma() ? view::withDecimalComma(view::valueParts(shown)) : view::valueParts(shown));
}

void MainWindow::apply(const Face& f) {
    // A key pressed in the phone's drawer brings back the pad; the letters keep it open, to type a whole name.
    if (f.action != KeyAction::Type && f.action != KeyAction::Shift && f.action != KeyAction::Store) drawerToggle_->setChecked(false);
    // STO armed: a variable letter stores what is on the screen under its name; any other key disarms it first.
    if (storing_) {
        storing_ = false;
        lcd_->setStoring(false);
        static const QStringList variables{"A", "B", "C", "D", "E", "F", "x", "y"};
        if (f.action == KeyAction::Type && variables.contains(f.insert)) {
            drawerToggle_->setChecked(false);
            const QString input = lcd_->input().trimmed();
            if (!input.isEmpty()) request(f.insert + " := " + input, false);
            lcd_->setFocus();
            return;
        }
    }
    switch (f.action) {
    case KeyAction::Insert: lcd_->insert(translated(f.insert)); break;
    case KeyAction::Type:
        lcd_->typeText(shifted_ ? f.insert.toUpper() : f.insert);
        shifted_ = false;
        relabelLetters();
        break;
    case KeyAction::Shift:
        shifted_ = !shifted_;
        relabelLetters();
        break;
    case KeyAction::Undo: lcd_->undo(); break;
    case KeyAction::Redo: lcd_->redo(); break;
    case KeyAction::Template: lcd_->insertTemplate(f.shape, f.insert); break;
    case KeyAction::Clear: lcd_->clear(); break;
    case KeyAction::Backspace: lcd_->backspace(); break;
    case KeyAction::Evaluate: evaluate(); break;
    case KeyAction::MemoryAdd: emit memoryAddRequested(); break;
    case KeyAction::MemorySubtract: emit memorySubtractRequested(); break;
    case KeyAction::MemoryStore: emit memoryStoreRequested(); break;
    case KeyAction::MemoryClear: emit memoryClearRequested(); break;
    case KeyAction::Left: lcd_->left(); break;
    case KeyAction::Right: lcd_->right(); break;
    case KeyAction::Up:
        if (!lcd_->up()) replay(historyIndex_ + 1);
        break;
    case KeyAction::Down:
        if (!lcd_->down()) replay(historyIndex_ - 1);
        break;
    case KeyAction::Store:
        storing_ = true;
        lcd_->setStoring(true);
        break;
    case KeyAction::Tool:
        if (f.opens == "percentages") {
            modes_->setCurrentRow(2);
            // The last result as the first value, in the copy form that reads back exactly.
            if (hasResult_ && !last_.error) {
                const QString value = view::copyText(last_, view::CopyForm::Value, types_[static_cast<std::size_t>(last_.type)]);
                percentFirst_->setText(settings::decimalComma() ? view::withDecimalComma(value) : value);
            }
            percentTarget_ = percentFirst_;
            percentFirst_->setFocus();
            return;
        }
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

void MainWindow::reevaluate() {
    updateKeys();
    if (settings::liveCalculation() && (previewShown_ || lcd_->input().trimmed() != lastExpression_)) requestPreview();
    else if (!lastExpression_.isEmpty() && !last_.error) request(lastExpression_, false);
    requestPercentages();
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
    for (const QString& id : common_) keys.append({"common:", directKey(id)});
    for (const auto& [prefix, key] : keys) {
        auto* button = findChild<QPushButton*>(prefix + key.id);
        const bool on = available(key.face, exact);
        button->setEnabled(on);
        button->setToolTip(on ? keyTip(key) : exactRefusal(key.face.label));
        button->setAccessibleName(keyName(key));
    }
    // A key with more faces also says which, and how to reach them.
    for (const auto& [id, others] : alternates()) {
        auto* button = findChild<QPushButton*>("key:" + id);
        if (!button->isEnabled()) continue;
        QStringList legends;
        for (const QString& other : others) legends << legend(directKey(other).face, settings::decimalComma());
        button->setToolTip(button->toolTip() + QLatin1Char('\n') + tr("Hold for: %1").arg(legends.join(QStringLiteral(", "))));
    }
    const bool natural = settings::conventions().log == calculate_core::Conventions::Log::Natural;
    for (const char* name : {"direct:log", "common:log"})
        if (auto* button = findChild<QPushButton*>(name); button && button->isEnabled())
            button->setToolTip(button->toolTip() + QLatin1Char('\n')
                               + (natural ? tr("logarithm (natural)") : tr("logarithm (base 10)")));
}
