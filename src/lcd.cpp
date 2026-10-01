#include "lcd.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>
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
    setAttribute(Qt::WA_InputMethodEnabled, false);
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
    entry_.setText(text);
    changed();
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

void Lcd::showMessage(const QString& message) {
    shown_ = message.isEmpty() ? Shown::Nothing : Shown::Message;
    message_ = message;
    changed();
}

QString Lcd::outputText() const {
    switch (shown_) {
    case Shown::Nothing: return {};
    case Shown::Message: return message_;
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
    // One input line, and room for a stacked fraction; taller results scroll.
    const qreal height = 2 * margin + spacing + QFontMetricsF(statusFont()).height() + QFontMetricsF(inputFont()).height()
                         + 2.4 * QFontMetricsF(outputFont()).height() + barHeight();
    return QSize(320, qCeil(height));
}

QFont Lcd::statusFont() const { return scaled(font(), 0.8); }
QFont Lcd::inputFont() const { return scaled(font(), 1.25); }
QFont Lcd::outputFont() const { return scaled(font(), 1.6); }

typeset::Box Lcd::inputBox() const {
    return typeset::paragraph({{entry_.text()}}, inputFont(), width() - 2 * margin);
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
        case Shown::Message: return typeset::paragraph({{message_}}, outputFont(), width);
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

namespace {

void draw(QPainter& painter, const typeset::Box& box, QPointF origin, const QColor& ink, const QColor& noise, const QRectF& clip) {
    for (const typeset::Run& run : box.runs) {
        const QPointF at = origin + run.origin;
        const QFontMetricsF m(run.font);
        if (at.y() + m.descent() < clip.top() || at.y() - m.ascent() > clip.bottom()) continue;
        painter.setFont(run.font);
        painter.setPen(run.role == typeset::Role::Noise ? noise : ink);
        painter.drawText(at, run.text);
    }
    painter.setPen(QPen(ink, 1.5));
    for (const QLineF& line : box.lines) painter.drawLine(line.translated(origin));
}

}  // namespace

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

    const typeset::Box input = inputBox();
    const QPointF inputOrigin(margin, margin + status.height() + input.ascent);
    draw(painter, input, inputOrigin, ink, ink, rect());
    if (hasFocus()) {
        const QStringList& pieces = entry_.pieces();
        const QString before = QStringList(pieces.mid(0, entry_.cursor())).join(QString());
        const QPointF at = inputOrigin + typeset::end(typeset::paragraph({{before}}, inputFont(), width() - 2 * margin));
        const QFontMetricsF m(inputFont());
        painter.setPen(QPen(ink, 1.5));
        painter.drawLine(QPointF(at.x(), at.y() - m.ascent()), QPointF(at.x(), at.y() + m.descent()));
    }

    const QRectF area = resultArea(input);
    painter.setClipRect(area);
    const qreal height = result_.ascent + result_.descent;
    const qreal baseline = height <= area.height() ? area.bottom() - result_.descent : area.top() + result_.ascent - scroll_->value();
    draw(painter, result_, QPointF(area.right() - result_.width, baseline), ink, noiseColor(), area);
}

void Lcd::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: emit evaluateRequested(); return;
    case Qt::Key_Backspace: backspace(); return;
    case Qt::Key_Escape: clear(); return;
    case Qt::Key_Left: left(); return;
    case Qt::Key_Right: right(); return;
    case Qt::Key_Up: emit historyRequested(1); return;
    case Qt::Key_Down: emit historyRequested(-1); return;
    default: break;
    }
    // Typed characters, translated to what the calculator's keys insert.
    QString piece;
    const QString text = event->text();
    const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (plain && text.size() == 1) {
        const QChar c = text[0];
        if (c >= '0' && c <= '9') piece = text;
        else if (c == '.' || c == ',') piece = QStringLiteral(".");
        else if (c == '+') piece = QStringLiteral("+");
        else if (c == '-') piece = QStringLiteral("−");
        else if (c == '*') piece = QStringLiteral("×");
        else if (c == '/') piece = QStringLiteral("÷");
        else if (c == '(' || c == ')') piece = text;
        else if (c == '=') {
            emit evaluateRequested();
            return;
        }
    }
    if (!piece.isEmpty()) {
        insert(piece);
        return;
    }
    event->ignore();  // anything else is not on the calculator
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
