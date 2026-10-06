#include "lcd.hpp"

#include "typing.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QWheelEvent>

namespace {

constexpr qreal margin = 8;
constexpr qreal spacing = 6;

// The colour a fraction `t` of the way from `a` to `b`.
QColor mix(const QColor& a, const QColor& b, qreal t) {
    const auto channel = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(channel(a.red(), b.red()), channel(a.green(), b.green()), channel(a.blue(), b.blue()));
}

QFont scaled(const QFont& base, qreal factor) {
    QFont f(Lcd::fontFamily());
    f.setPixelSize(qRound(QFontInfo(base).pixelSize() * factor));
    return f;
}

}  // namespace

Lcd::Lcd(QWidget* parent) : QWidget(parent) {
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
}

void Lcd::insert(const QString& piece) {
    entry_.insert(piece);
    changed();
}

void Lcd::insertTemplate(Template kind, const QString& fill) {
    entry_.insertTemplate(kind);
    if (!fill.isEmpty()) {
        for (const QChar c : fill) entry_.insert(c);
        entry_.right();
    }
    changed();
}

bool Lcd::up() {
    const bool moved = entry_.up();
    update();
    return moved;
}

bool Lcd::down() {
    const bool moved = entry_.down();
    update();
    return moved;
}

void Lcd::backspace() {
    entry_.backspace();
    changed();
}

void Lcd::left() {
    entry_.left();
    update();
}

void Lcd::right() {
    entry_.right();
    update();
}

void Lcd::setInput(const QString& text) {
    entry_.setRoot(typing::read(text));
    changed();
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

void Lcd::clear() {
    entry_.clear();
    shown_ = Shown::Nothing;
    changed();
}

void Lcd::showValue(const view::ValueParts& parts) {
    shown_ = Shown::Value;
    value_ = parts;
    changed();
}

void Lcd::showExact(const view::FractionParts& parts) {
    shown_ = Shown::Exact;
    exact_ = parts;
    changed();
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
                         + 2.4 * QFontMetricsF(outputFont()).height() + barHeight();
    return QSize(320, qCeil(height));
}

QFont Lcd::statusFont() const { return scaled(font(), 0.8); }
QFont Lcd::inputFont() const { return scaled(font(), 1.25); }
QFont Lcd::outputFont() const { return scaled(font(), 1.6); }

typeset::Box Lcd::inputBox(QRectF* caret) const { return typeset::input(entry_, inputFont(), caret); }

// An input wider than the screen slides left, as in a text field, so the cursor stays in view.
QPointF Lcd::inputOrigin(const typeset::Box& input, const QRectF& caret) const {
    const qreal room = width() - 2 * margin;
    const qreal shift = caret.left() > room ? room - caret.left() - 2 : 0;
    return {margin + shift, margin + QFontMetricsF(statusFont()).height() + input.ascent};
}

void Lcd::setEntry(const Entry& entry) {
    entry_ = entry;
    changed();
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
        entry_.deleteForward();
        changed();
        return;
    case Qt::Key_Escape: clear(); return;
    case Qt::Key_Left:
        finishName();
        left();
        return;
    case Qt::Key_Right:
        finishName();
        right();
        return;
    case Qt::Key_Home:
        entry_.home();
        update();
        return;
    case Qt::Key_End:
        entry_.end();
        update();
        return;
    case Qt::Key_Up:
        if (!up()) emit historyRequested(1);
        return;
    case Qt::Key_Down:
        if (!down()) emit historyRequested(-1);
        return;
    default: break;
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
    for (const QChar c : text) typed = typing::typeCharacter(entry_, c) || typed;
    if (typed) changed();
    else event->ignore();
}

// An input method's text (a dead key's ^, a phone's keyboard) is typed like the keyboard's; the text it is
// still composing is not shown.
void Lcd::inputMethodEvent(QInputMethodEvent* event) {
    bool typed = false;
    for (const QChar c : event->commitString()) typed = typing::typeCharacter(entry_, c) || typed;
    if (typed) changed();
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
    if (event->type() == QEvent::LanguageChange) setMemory(memory_);  // its tooltip
    QWidget::changeEvent(event);
}
