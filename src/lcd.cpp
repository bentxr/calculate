#include "lcd.hpp"

#include "typing.hpp"
#ifdef Q_OS_WASM
#include "webclipboard.hpp"
#endif

#include <QAccessible>
#include <QAccessibleWidget>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QWheelEvent>

namespace {

constexpr qreal margin = 8;
constexpr qreal spacing = 6;
constexpr qreal resultScale = 1.6;       // the result's size, relative to the widget's font
constexpr qreal provisionalScale = 1.2;  // smaller while the expression is still being typed

// The colour a fraction `t` of the way from `a` to `b`.
QColor mix(const QColor& a, const QColor& b, qreal t) {
    const auto channel = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(channel(a.red(), b.red()), channel(a.green(), b.green()), channel(a.blue(), b.blue()));
}

// The pieces and characters that need something on their left: after a result, that is Ans.
bool needsLeftOperand(const QString& piece) {
    static const QStringList operators{"+", "−", "×", "÷", "!", "%"};
    return operators.contains(piece);
}

bool needsLeftOperand(QChar typed) { return QStringLiteral("+-*/^!%²³×÷−").contains(typed); }

// What a screen reader hears: the input as the screen's value, the result as its description.
class LcdAccessible : public QAccessibleWidget {
public:
    explicit LcdAccessible(Lcd* lcd) : QAccessibleWidget(lcd, QAccessible::EditableText) {}

    QString text(QAccessible::Text t) const override {
        const auto* lcd = static_cast<const Lcd*>(widget());
        switch (t) {
        case QAccessible::Name: return Lcd::tr("Calculator screen");
        case QAccessible::Value: return lcd->input();
        case QAccessible::Description: return lcd->outputText();
        default: return QAccessibleWidget::text(t);
        }
    }
};

QAccessibleInterface* lcdAccessible(const QString& className, QObject* object) {
    if (className == QLatin1String("Lcd")) return new LcdAccessible(static_cast<Lcd*>(object));
    return nullptr;
}

QFont scaled(const QFont& base, qreal factor) {
    QFont f(Lcd::fontFamily());
    f.setPixelSize(qRound(QFontInfo(base).pixelSize() * factor));
    return f;
}

}  // namespace

Lcd::Lcd(QWidget* parent) : QWidget(parent) {
    static const bool accessible = (QAccessible::installFactory(lcdAccessible), true);
    Q_UNUSED(accessible)
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);  // never shorter than sizeHint()
#ifdef Q_OS_WASM
    setAttribute(Qt::WA_InputMethodEnabled, false);  // a phone's keyboard would cover the keypad
#else
    setAttribute(Qt::WA_InputMethodEnabled, true);
#endif
    scroll_ = new QScrollBar(Qt::Vertical, this);
    scroll_->hide();
    bar_ = new QWidget(this);
    barLayout_ = new QHBoxLayout(bar_);
    barLayout_->setContentsMargins(0, 0, 0, 0);
    barLayout_->addStretch();
    connect(scroll_, &QScrollBar::valueChanged, this, qOverload<>(&QWidget::update));
    buildEditMenu();
    retranslate();
}

void Lcd::buildEditMenu() {
    editMenu_ = new QMenu(this);
    const auto add = [this](const char* name, void (Lcd::*slot)()) {
        QAction* action = editMenu_->addAction(QString());
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    QAction* undo = add("edit:undo", &Lcd::undo);
    QAction* redo = add("edit:redo", &Lcd::redo);
    editMenu_->addSeparator();
    QAction* cut = add("edit:cut", &Lcd::cut);
    add("edit:copy", &Lcd::copy);
    add("edit:paste", &Lcd::requestPaste);
    editMenu_->addSeparator();
    add("edit:selectAll", &Lcd::selectAll);
    connect(editMenu_, &QMenu::aboutToShow, this, [this, undo, redo, cut] {
        undo->setEnabled(undo_.canUndo());
        redo->setEnabled(undo_.canRedo());
        cut->setEnabled(entry_.hasSelection());
    });
}

void Lcd::retranslate() {
    findChild<QAction*>("edit:undo")->setText(tr("Undo"));
    findChild<QAction*>("edit:redo")->setText(tr("Redo"));
    findChild<QAction*>("edit:cut")->setText(tr("Cut"));
    findChild<QAction*>("edit:copy")->setText(tr("Copy"));
    findChild<QAction*>("edit:paste")->setText(tr("Paste"));
    findChild<QAction*>("edit:selectAll")->setText(tr("Select all"));
}

void Lcd::insert(const QString& piece) {
    edit([&] {
        startEditing(needsLeftOperand(piece));
        entry_.insert(piece);
    });
}

void Lcd::insertTemplate(Template kind, const QString& fill) {
    edit([&] {
        startEditing(kind == Template::Power);
        entry_.insertTemplate(kind);
        if (!fill.isEmpty()) {
            for (const QChar c : fill) entry_.insert(c);
            entry_.right();
        }
    });
}

bool Lcd::up() {
    fresh_ = false;
    const bool moved = entry_.up();
    update();
    return moved;
}

bool Lcd::down() {
    fresh_ = false;
    const bool moved = entry_.down();
    update();
    return moved;
}

void Lcd::backspace() {
    fresh_ = false;
    edit([this] { entry_.backspace(); });
}

void Lcd::left() {
    fresh_ = false;
    entry_.left();
    update();
}

void Lcd::right() {
    fresh_ = false;
    entry_.right();
    update();
}

void Lcd::setInput(const QString& text) {
    fresh_ = false;
    edit([&] { entry_.setRoot(typing::read(text)); }, false);
}

void Lcd::finishName() {
    typing::finishName(entry_);
    changed();
}

void Lcd::setSystemKeyboard(bool on) {
    setAttribute(Qt::WA_InputMethodEnabled, on);
    if (on) {
        setFocus();
        QGuiApplication::inputMethod()->show();
    } else {
        QGuiApplication::inputMethod()->hide();
    }
}

void Lcd::selectAll() {
    fresh_ = false;
    entry_.selectAll();
    update();
}

QRectF Lcd::caretRectAt(const Position& p) const {
    QRectF caret;
    const typeset::Box input = inputBox(&caret);
    const QPointF origin = inputOrigin(input, caret);
    for (const typeset::Mark& mark : input.marks)
        if (mark.at == p) return mark.caret.translated(origin);
    return {};
}

Position Lcd::positionAt(QPointF point) const {
    QRectF caret;
    const typeset::Box input = inputBox(&caret);
    return typeset::hit(input, point - inputOrigin(input, caret));
}

void Lcd::cut() {
    if (!entry_.hasSelection()) return;
    copy();
    edit([this] { entry_.backspace(); });  // removes the selection
}

void Lcd::copy() {
    if (entry_.hasSelection()) QGuiApplication::clipboard()->setText(entry_.selectedText());
    else emit copyRequested();
}

void Lcd::pasteText(const QString& text) {
    QStringList lines;
    for (QString line : text.split('\n')) {
        line.remove('\r');
        if (!line.trimmed().isEmpty()) lines << line;
    }
    if (lines.isEmpty()) return;
    edit([&] {
        startEditing(false);  // a pasted expression is a new one
        typing::paste(entry_, lines.first());
    });
    if (lines.size() > 1) emit pastedFirstLine(static_cast<int>(lines.size()));
}

void Lcd::requestPaste() {
#ifdef Q_OS_WASM
    webclipboard::readText(this, [this](const QString& text) { pasteText(text); }, [this] { emit pasteRefused(); });
#else
    pasteText(QGuiApplication::clipboard()->text());
#endif
}

void Lcd::clear() {
    fresh_ = false;
    shown_ = Shown::Nothing;
    edit([this] { entry_.clear(); });
    changed();  // the result went even when the input was already empty
}

void Lcd::showValue(const view::ValueParts& parts) {
    shown_ = Shown::Value;
    value_ = parts;
    changed();
    announceResult();
}

void Lcd::showExact(const view::FractionParts& parts) {
    shown_ = Shown::Exact;
    exact_ = parts;
    changed();
    announceResult();
}

void Lcd::setProvisional(bool provisional) {
    if (provisional == provisional_) return;
    provisional_ = provisional;
    changed();
}

void Lcd::setMarked(int begin, int end) {
    if (end <= begin) {
        clearMarked();
        return;
    }
    marked_ = entry_;
    Position last = entry_.positionAt(end - 1);
    ++last.index;  // so that the last item is included
    marked_.select(entry_.positionAt(begin), last);
    update();
}

void Lcd::clearMarked() {
    marked_ = Entry();
    update();
}

QSize Lcd::resultSize() const { return {qCeil(result_.width), qCeil(result_.ascent + result_.descent)}; }

void Lcd::announceResult() {
    if (!QAccessible::isActive()) return;
    QAccessibleEvent event(this, QAccessible::DescriptionChanged);
    QAccessible::updateAccessibility(&event);
}

void Lcd::clearResult() {
    shown_ = Shown::Nothing;
    changed();
}

QString Lcd::outputText() const {
    switch (shown_) {
    case Shown::Nothing: return {};
    case Shown::Value: {
        QString s = value_.trusted;
        if (!value_.noise.isEmpty()) s += "|" + value_.noise;
        if (!value_.exponent.isEmpty()) s += "×10^" + value_.exponent;
        return s;
    }
    case Shown::Exact: {
        if (exact_.denominator == "1") return exact_.sign + exact_.numerator;
        QString s = exact_.sign + exact_.numerator + "/" + exact_.denominator;
        if (!exact_.decimal.isEmpty()) s += " = " + exact_.sign + exact_.decimal;
        if (!exact_.recurring.isEmpty()) s += "(" + exact_.recurring + ")";
        return s;
    }
    }
    return {};
}

void Lcd::setMemory(const QString& memory) {
    memory_ = memory;
    setToolTip(memory.isEmpty() ? QString() : tr("M = %1").arg(memory));
    update();
}

QString Lcd::statusText() const { return memory_.isEmpty() ? QString() : QStringLiteral("M"); }

// JetBrains Mono, loaded once from the resources; the system's fixed font if that ever fails.
QString Lcd::fontFamily() {
    static const QString family = [] {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"));
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        return families.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::FixedFont).family() : families.first();
    }();
    return family;
}

bool Lcd::dark() const { return palette().color(QPalette::Window).lightness() < 128; }
QColor Lcd::background() const { return dark() ? QColor(0x1f, 0x26, 0x21) : QColor(0xc8, 0xd3, 0xbf); }
QColor Lcd::ink() const { return dark() ? QColor(0xd4, 0xe2, 0xcc) : QColor(0x1c, 0x24, 0x1a); }
QColor Lcd::noiseColor() const { return mix(ink(), background(), 0.5); }

void Lcd::addToBar(QWidget* widget, bool right) {
    if (right) barLayout_->addWidget(widget);
    else barLayout_->insertWidget(leftOfBar_++, widget);  // before the stretch
    changed();
}

int Lcd::barHeight() const { return barLayout_->count() > 1 ? bar_->sizeHint().height() : 0; }  // more than the stretch

QSize Lcd::sizeHint() const {
    // An input with a fraction, and a result with one; taller results scroll.
    const qreal height = 2 * margin + spacing + QFontMetricsF(statusFont()).height() + 2.2 * QFontMetricsF(inputFont()).height()
                         + 2.4 * QFontMetricsF(scaled(font(), resultScale)).height() + barHeight();
    return QSize(320, qCeil(height));
}

QFont Lcd::statusFont() const { return scaled(font(), 0.8); }
QFont Lcd::inputFont() const { return scaled(font(), 1.25); }
QFont Lcd::outputFont() const { return scaled(font(), provisional_ ? provisionalScale : resultScale); }

typeset::Box Lcd::inputBox(QRectF* caret) const { return typeset::input(entry_, inputFont(), caret); }

// An input wider than the screen slides left, as in a text field, so the cursor stays in view.
QPointF Lcd::inputOrigin(const typeset::Box& input, const QRectF& caret) const {
    const qreal room = width() - 2 * margin;
    const qreal shift = caret.left() > room ? room - caret.left() - 2 : 0;
    return {margin + shift, margin + QFontMetricsF(statusFont()).height() + input.ascent};
}

void Lcd::setEntry(const Entry& entry) {
    fresh_ = false;
    edit([&] { entry_ = entry; }, false);
}

void Lcd::edit(const std::function<void()>& change, bool byUser) {
    const Entry before = entry_;
    change();
    if (entry_ == before) return;
    undo_.record(before);
    marked_ = Entry();  // its bytes were those of the input before
    changed();
    if (QAccessible::isActive()) {
        QAccessibleValueChangeEvent event(this, input());
        QAccessible::updateAccessibility(&event);
    }
    if (byUser) emit inputChanged();
}

void Lcd::startEditing(bool needsLeftOperand) {
    if (!fresh_) return;
    fresh_ = false;
    if (needsLeftOperand) entry_.setRoot({Item{Template::Text, QStringLiteral("Ans"), {}}});
    else entry_.clear();
}

void Lcd::undo() {
    fresh_ = false;
    if (!undo_.undo(entry_)) return;
    marked_ = Entry();
    changed();
    emit inputChanged();
}

void Lcd::redo() {
    fresh_ = false;
    if (!undo_.redo(entry_)) return;
    marked_ = Entry();
    changed();
    emit inputChanged();
}

QRectF Lcd::resultArea(const typeset::Box& input) const {
    const qreal top = margin + QFontMetricsF(statusFont()).height() + input.ascent + input.descent + spacing;
    const qreal right = scroll_->isVisible() ? width() - scroll_->width() : width() - margin;
    return QRectF(margin, top, right - margin, height() - top - margin - barHeight());
}

void Lcd::changed() {
    const auto layOut = [this](qreal width) {
        switch (shown_) {
        case Shown::Nothing: return typeset::Box{};
        case Shown::Value: return typeset::value(value_, outputFont(), width);
        case Shown::Exact: return typeset::exact(exact_, outputFont(), width);
        }
        return typeset::Box{};
    };
    bar_->setGeometry(qRound(margin / 2), height() - barHeight() - 2, width() - qRound(margin), barHeight());
    scroll_->hide();
    const typeset::Box input = inputBox();
    QRectF area = resultArea(input);
    result_ = layOut(area.width());
    if (result_.ascent + result_.descent > area.height()) {  // too tall: make room for the scroll bar
        scroll_->setGeometry(width() - scroll_->sizeHint().width(), qRound(area.top()), scroll_->sizeHint().width(), qRound(area.height()));
        scroll_->show();
        area = resultArea(input);
        result_ = layOut(area.width());
        scroll_->setRange(0, qCeil(result_.ascent + result_.descent - area.height()));
        scroll_->setPageStep(qRound(area.height()));
        scroll_->setSingleStep(qRound(QFontMetricsF(outputFont()).lineSpacing()));
        scroll_->setValue(0);
    }
    update();
}

void Lcd::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(mix(background(), ink(), 0.35), 1));
    painter.setBrush(background());
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    const QColor ink = this->ink();

    const QFontMetricsF status(statusFont());
    painter.setFont(statusFont());
    painter.setPen(ink);
    painter.drawText(QPointF(margin, margin + status.ascent()), statusText());

    QRectF caret;
    const typeset::Box input = inputBox(&caret);
    const QPointF origin = inputOrigin(input, caret);
    const qreal room = width() - 2 * margin;
    painter.save();
    painter.setClipRect(QRectF(margin, 0, room, height()));
    const QRectF selection = typeset::selectionRect(input, entry_);
    if (!selection.isEmpty()) painter.fillRect(selection.translated(origin), mix(ink, background(), 0.75));  // readable in both themes
    const QRectF marked = typeset::selectionRect(input, marked_);
    if (!marked.isEmpty()) {
        painter.setPen(QPen(ink, 1, Qt::DotLine));
        const QRectF under = marked.translated(origin);
        painter.drawLine(under.bottomLeft(), under.bottomRight());
    }
    typeset::paint(painter, input, origin, ink, ink, rect());
    if (hasFocus()) {
        painter.setPen(QPen(ink, 1.5));
        const QRectF at = caret.translated(origin);
        painter.drawLine(at.topLeft(), at.bottomLeft());
    }
    painter.restore();

    const QRectF area = resultArea(input);
    painter.setClipRect(area);
    const qreal height = result_.ascent + result_.descent;
    const qreal baseline = height <= area.height() ? area.bottom() - result_.descent : area.top() + result_.ascent - scroll_->value();
    typeset::paint(painter, result_, QPointF(area.right() - result_.width, baseline), ink, noiseColor(), area);
}

void Lcd::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        finishName();
        emit evaluateRequested();
        return;
    case Qt::Key_Backspace: backspace(); return;
    case Qt::Key_Delete:
        fresh_ = false;
        edit([this] { entry_.deleteForward(); });
        return;
    case Qt::Key_Escape: clear(); return;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End: {
        fresh_ = false;
        finishName();
        const bool select = event->modifiers() & Qt::ShiftModifier;  // Shift with a movement selects
        const int key = event->key();
        if (key == Qt::Key_Left) {
            if (select) entry_.extendLeft();
            else entry_.left();
        } else if (key == Qt::Key_Right) {
            if (select) entry_.extendRight();
            else entry_.right();
        } else if (key == Qt::Key_Home) {
            if (select) entry_.extendHome();
            else entry_.home();
        } else {
            if (select) entry_.extendEnd();
            else entry_.end();
        }
        update();
        return;
    }
    case Qt::Key_Up:
        if (!up()) emit historyRequested(1);
        return;
    case Qt::Key_Down:
        if (!down()) emit historyRequested(-1);
        return;
    case Qt::Key_PageUp:  // the history, even inside a fraction
        fresh_ = false;
        emit historyRequested(1);
        return;
    case Qt::Key_PageDown:
        fresh_ = false;
        emit historyRequested(-1);
        return;
    default: break;
    }
    // Redo is checked first and spelled out: not every platform binds both Ctrl+Y and Ctrl+Shift+Z.
    const auto modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if (event->matches(QKeySequence::Redo) || (event->key() == Qt::Key_Y && modifiers == Qt::ControlModifier) ||
        (event->key() == Qt::Key_Z && modifiers == (Qt::ControlModifier | Qt::ShiftModifier))) {
        redo();
        return;
    }
    if (event->matches(QKeySequence::Undo)) {
        undo();
        return;
    }
    // Ctrl+V reads the clipboard directly, in the browser too: its own paste event has filled it.
    if (event->matches(QKeySequence::Cut)) {
        cut();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copy();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteText(QGuiApplication::clipboard()->text());
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        return;
    }
    const QString text = event->text();
    if (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) {  // shortcuts are not typed
        event->ignore();
        return;
    }
    if ((event->modifiers() & Qt::KeypadModifier) && (text == "," || text == ".")) {  // the keypad's decimal key
        insert(QStringLiteral("."));
        return;
    }
    if (text == "=") {
        const Row& row = entry_.currentRow();
        if (entry_.cursor() > 0 && row[static_cast<std::size_t>(entry_.cursor() - 1)].text == ":") insert("=");  // :=
        else emit evaluateRequested();
        return;
    }
    bool typed = false;
    edit([&] {
        if (!text.isEmpty() && text[0].isPrint()) startEditing(needsLeftOperand(text[0]));
        for (const QChar c : text) typed = typing::typeCharacter(entry_, c) || typed;
    });
    if (!typed) event->ignore();
}

// A click places the cursor at the nearest place, Shift+click selects up to it, a drag selects. (A tap
// arrives as a click: touches become mouse events for a widget that doesn't take them.)
void Lcd::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    fresh_ = false;
    const Position at = positionAt(event->position());
    if (event->modifiers() & Qt::ShiftModifier) {
        dragFrom_ = entry_.hasSelection() ? Position{entry_.path(), entry_.anchor()} : entry_.position();
        entry_.select(dragFrom_, at);
    } else {
        dragFrom_ = at;
        entry_.setPosition(at);
    }
    dragging_ = true;
    update();
}

void Lcd::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    entry_.select(dragFrom_, positionAt(event->position()));
    update();
}

void Lcd::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) dragging_ = false;
    QWidget::mouseReleaseEvent(event);
}

// An input method's text (a dead key's ^, a phone's keyboard) is typed like the keyboard's; the text it is
// still composing is not shown.
void Lcd::inputMethodEvent(QInputMethodEvent* event) {
    const QString text = event->commitString();
    edit([&] {
        if (!text.isEmpty()) startEditing(needsLeftOperand(text[0]));
        for (const QChar c : text) typing::typeCharacter(entry_, c);
    });
    event->accept();
}

QVariant Lcd::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query) {
    case Qt::ImEnabled: return true;
    case Qt::ImCursorRectangle: {
        QRectF caret;
        const typeset::Box input = inputBox(&caret);
        return caret.translated(inputOrigin(input, caret)).toAlignedRect();
    }
    case Qt::ImHints: return static_cast<int>(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    default: return QWidget::inputMethodQuery(query);
    }
}

void Lcd::wheelEvent(QWheelEvent* event) {
    if (!scroll_->isVisible()) {
        event->ignore();
        return;
    }
    scroll_->setValue(scroll_->value() - event->angleDelta().y() / 120 * scroll_->singleStep());
}

void Lcd::resizeEvent(QResizeEvent*) { changed(); }

void Lcd::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        setMemory(memory_);  // its tooltip
        retranslate();
    }
    QWidget::changeEvent(event);
}

void Lcd::contextMenuEvent(QContextMenuEvent* event) { editMenu_->popup(event->globalPos()); }
