#include "inspector.hpp"

#include "icons.hpp"
#include "presenter.hpp"

#include <QAction>
#include <QClipboard>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxHighlighter>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

using namespace calculate_core;

namespace {

// A field that wraps anywhere, about three lines tall, plain text only, and takes only `allowed` characters
// (typed or pasted); never Enter.
class FieldEdit : public QPlainTextEdit {
public:
    FieldEdit(QWidget* parent, QString allowed) : QPlainTextEdit(parent), allowed_(std::move(allowed)) {
        setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);  // groups of bits stay whole when they fit
        setTabChangesFocus(true);
        setFixedHeight(fontMetrics().lineSpacing() * 3 + 2 * frameWidth() + 8);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        const QString typed = event->text();
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) return;
        if (!typed.isEmpty() && typed[0].isPrint() && filtered(typed) != typed) return;
        QPlainTextEdit::keyPressEvent(event);
    }
    void insertFromMimeData(const QMimeData* source) override { insertPlainText(filtered(source->text())); }

public:
    void type(const QString& text) { insertPlainText(filtered(text)); }  // a key of the tool's keypad

private:
    QString filtered(const QString& text) const {
        QString s;
        for (const QChar c : text)
            if (allowed_.contains(c)) s += c;
        return s;
    }
    QString allowed_;
};

// A value that wraps anywhere (a binary512 value has hundreds of digits and no spaces), so the page fits a phone.
// Its text is copied from a context menu, since it paints itself.
class WrapLabel : public QLabel {
public:
    explicit WrapLabel(QWidget* parent) : QLabel(parent) {
        setTextFormat(Qt::PlainText);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        setContextMenuPolicy(Qt::ActionsContextMenu);
        auto* copy = new QAction(this);
        copy->setObjectName("copyValue");
        connect(copy, &QAction::triggered, this, [this] { QGuiApplication::clipboard()->setText(text()); });
        addAction(copy);
    }
    QAction* copyAction() const { return actions().first(); }
    QSize minimumSizeHint() const override { return {fontMetrics().averageCharWidth() * 8, heightForWidth(fontMetrics().averageCharWidth() * 8)}; }
    QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text()) + 1, fontMetrics().height()}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return text().isEmpty() ? fontMetrics().height() : static_cast<int>(laidOut(width)); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::WindowText));
        QTextLayout layout(text(), font());
        lineUp(layout, width());
        layout.draw(&painter, QPointF(0, 0));
    }

private:
    // Lines of the text broken anywhere at this width; returns the height.
    qreal lineUp(QTextLayout& layout, int width) const {
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);  // words stay whole when they fit
        layout.setTextOption(option);
        layout.beginLayout();
        qreal y = 0;
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(qMax(1, width));
            line.setPosition(QPointF(0, y));
            y += line.height();
        }
        layout.endLayout();
        return y;
    }
    qreal laidOut(int width) const {
        QTextLayout layout(text(), font());
        return lineUp(layout, width);
    }
};

// Colours the binary digits by field: the sign, w exponent digits, then the fraction (spaces don't count).
class BitsHighlighter : public QSyntaxHighlighter {
public:
    BitsHighlighter(QTextDocument* document, int exponentBits, bool dark)
        : QSyntaxHighlighter(document), exponentBits_(exponentBits), dark_(dark) {}
    void setLayout(int exponentBits, bool dark) {
        exponentBits_ = exponentBits;
        dark_ = dark;
        rehighlight();
    }

protected:
    void highlightBlock(const QString& text) override {
        const view::BitColours colours = view::bitColours(dark_);
        int position = 0;
        for (int i = 0; i < text.size(); ++i) {
            if (text[i] != '0' && text[i] != '1') continue;
            const QString& colour = position == 0 ? colours.sign : position <= exponentBits_ ? colours.exponent : colours.fraction;
            setFormat(i, 1, QColor(colour));
            ++position;
        }
    }

private:
    int exponentBits_;
    bool dark_;
};

// Groups of four from the left, joined by spaces.
QString groupsOf4(const QString& text) {
    QString s;
    for (int i = 0; i < text.size(); ++i) {
        if (i > 0 && i % 4 == 0) s += ' ';
        s += text[i];
    }
    return s;
}

QString power(long long e) { return QStringLiteral("2^") + (e < 0 ? QString(QChar(0x2212)) + QString::number(-e) : QString::number(e)); }

bool dark(const QWidget* w) { return w->palette().color(QPalette::Window).lightness() < 128; }

const char* const outputs[] = {"class", "parts", "value", "error", "ulp", "below", "above"};

}  // namespace

BitStrip::BitStrip(QWidget* parent) : QWidget(parent) {
    setObjectName("inspectorBitStrip");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void BitStrip::setPattern(const QString& sign, const QString& exponent, const QString& fraction) {
    bits_ = sign + exponent + fraction;
    exponentBits_ = static_cast<int>(exponent.size());
    boxes_ = layOut(width());
    updateGeometry();
    update();
}

std::vector<QRect> BitStrip::layOut(int width) const {
    const QFontMetrics m(font());
    const int w = m.horizontalAdvance('0') + 2;
    const int h = m.height() + 4;
    std::vector<QRect> boxes;
    int x = 0, y = 0;
    for (int i = 0; i < bits_.size(); ++i) {
        // the bit's place in its field: the sign, then the exponent, then the fraction
        const int start = i == 0 ? 0 : i <= exponentBits_ ? 1 : 1 + exponentBits_;
        if (i > 0 && i == start) x += w;                     // between fields
        else if (i > 0 && (i - start) % 4 == 0) x += w / 2;  // between groups
        if (width > 0 && x + w > width && x > 0) {
            x = 0;
            y += h;
        }
        boxes.emplace_back(x, y, w, h);
        x += w;
    }
    return boxes;
}

int BitStrip::heightForWidth(int width) const {
    const std::vector<QRect> boxes = layOut(width);
    return boxes.empty() ? QFontMetrics(font()).height() + 4 : boxes.back().bottom() + 1;
}

QSize BitStrip::sizeHint() const { return {QFontMetrics(font()).horizontalAdvance('0') * 40, heightForWidth(width())}; }

QPoint BitStrip::bitCenter(int index) const { return boxes_.at(static_cast<std::size_t>(index)).center(); }

void BitStrip::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    const view::BitColours colours = view::bitColours(palette().color(QPalette::Window).lightness() < 128);
    for (int i = 0; i < bits_.size(); ++i) {
        const QString& colour = i == 0 ? colours.sign : i <= exponentBits_ ? colours.exponent : colours.fraction;
        painter.setPen(isEnabled() ? QColor(colour) : palette().color(QPalette::Disabled, QPalette::Text));
        painter.drawText(boxes_[static_cast<std::size_t>(i)], Qt::AlignCenter, QString(bits_[i]));
    }
}

void BitStrip::mouseReleaseEvent(QMouseEvent* event) {
    for (std::size_t i = 0; i < boxes_.size(); ++i)
        if (boxes_[i].contains(event->position().toPoint())) {
            emit flipped(static_cast<int>(i));
            return;
        }
}

void BitStrip::resizeEvent(QResizeEvent* event) {
    boxes_ = layOut(width());
    QWidget::resizeEvent(event);
}

Inspector::Inspector(QWidget* parent) : QWidget(parent), formats_(floatFormats()) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    format_ = new QComboBox(this);
    format_->setObjectName("inspectorFormat");
    format_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    format_->setMinimumContentsLength(10);
    for (std::size_t i = 0; i < formats_.size(); ++i) format_->addItem(QString());
    decimal_ = new FieldEdit(this, QStringLiteral("0123456789.eE+-\u2212infaINFA\u221E "));
    decimal_->setObjectName("inspectorDecimal");
    binary_ = new FieldEdit(this, QStringLiteral("01 "));
    binary_->setObjectName("inspectorBinary");
    hex_ = new FieldEdit(this, QStringLiteral("0123456789abcdefABCDEFxX "));
    hex_->setObjectName("inspectorHex");
    for (const char* key : {"format", "decimal", "binary", "hex"}) {
        auto* caption = new QLabel(this);
        caption->setObjectName(QStringLiteral("inspectorLabel:") + key);
        captions_[key] = caption;
    }
    form->addRow(captions_["format"], format_);
    form->addRow(captions_["decimal"], decimal_);
    form->addRow(captions_["binary"], binary_);
    strip_ = new BitStrip(this);
    form->addRow(QString(), strip_);
    form->addRow(captions_["hex"], hex_);
    layout->addLayout(form);
    // A keypad, for touch screens: it types into the field that last had the focus.
    keysToggle_ = new QToolButton(this);
    keysToggle_->setObjectName("inspectorKeysToggle");

    keysToggle_->setCheckable(true);
    keysToggle_->setChecked(true);
    keysToggle_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(keysToggle_, 0, Qt::AlignLeft);
    auto* keys = new QWidget(this);
    keys->setObjectName("inspectorKeys");
    auto* keyGrid = new QGridLayout(keys);
    keyGrid->setContentsMargins(0, 0, 0, 0);
    const QStringList labels{"7", "8", "9", "A", "B", "C", "4", "5", "6", "D", "E", "F", "1", "2", "3", ".", "e",
                             QStringLiteral("−"), "0", QStringLiteral("⌫")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* key = new QPushButton(labels[i], keys);
        key->setObjectName(QStringLiteral("inspectorKey:") + labels[i]);
        key->setFocusPolicy(Qt::NoFocus);
        key->setMinimumWidth(32);  // below the style's default, as the main keys: six fit a phone
        keyGrid->addWidget(key, i / 6, i % 6);
        const QString label = labels[i];
        connect(key, &QPushButton::clicked, this, [this, label] {
            auto* field = static_cast<FieldEdit*>(lastField_);
            if (label == QStringLiteral("⌫")) field->textCursor().deletePreviousChar();
            else field->type(label == QStringLiteral("−") ? QStringLiteral("-") : label);
        });
    }
    layout->addWidget(keys);
    connect(keysToggle_, &QToolButton::toggled, keys, &QWidget::setVisible);
    message_ = new QLabel(this);
    message_->setObjectName("inspectorMessage");
    message_->setWordWrap(true);
    layout->addWidget(message_);
    auto* grid = new QGridLayout;
    int row = 0;
    for (const char* key : outputs) {
        auto* caption = new QLabel(this);
        caption->setObjectName(QStringLiteral("inspectorLabel:") + key);
        QFont bold = caption->font();
        bold.setBold(true);
        caption->setFont(bold);
        caption->setWordWrap(true);  // long captions ("Conversion error…") wrap on a phone
        auto* value = new WrapLabel(this);
        value->setObjectName(QStringLiteral("inspector:") + key);
        captions_[key] = caption;
        values_[key] = value;
        grid->addWidget(caption, row, 0, Qt::AlignTop);
        grid->addWidget(value, row, 1);
        ++row;
    }
    down_ = new QPushButton(QStringLiteral("◄"), this);
    down_->setObjectName("inspectorDown");
    up_ = new QPushButton(QStringLiteral("►"), this);
    up_->setObjectName("inspectorUp");
    for (QPushButton* step : {down_, up_}) step->setMinimumWidth(32);
    grid->addWidget(down_, row - 2, 2, Qt::AlignTop);
    grid->addWidget(up_, row - 1, 2, Qt::AlignTop);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addStretch();
    lastField_ = decimal_;
    for (QPlainTextEdit* field : {decimal_, binary_, hex_}) field->installEventFilter(this);
    highlighter_ = new BitsHighlighter(binary_->document(), 11, dark(this));
    connect(decimal_, &QPlainTextEdit::textChanged, this, [this] {
        if (!updating_) convertDecimal();
    });
    connect(binary_, &QPlainTextEdit::textChanged, this, [this] {
        if (!updating_) convertBits(binary_, 2);
    });
    connect(hex_, &QPlainTextEdit::textChanged, this, [this] {
        if (!updating_) convertBits(hex_, 16);
    });
    connect(format_, &QComboBox::currentIndexChanged, this, [this] {
        recolour();
        if (!updating_) formatChanged();
    });
    connect(strip_, &BitStrip::flipped, this, [this](int index) {  // that bit XORed into the pattern, reloaded
        QString bits = QString::fromStdString(current_.stored.sign + current_.stored.exponent + current_.stored.fraction);
        if (index < 0 || index >= bits.size()) return;
        bits[index] = bits[index] == '0' ? '1' : '0';
        const FloatInspection inspection = inspectBits(format(), bits.toStdString(), 2);
        if (!inspection.error) display(inspection, nullptr);
    });
    connect(down_, &QPushButton::clicked, this, [this] { loadBits(format_->currentIndex(), QString::fromStdString(current_.below.hex)); });
    connect(up_, &QPushButton::clicked, this, [this] { loadBits(format_->currentIndex(), QString::fromStdString(current_.above.hex)); });
    down_->setEnabled(false);
    up_->setEnabled(false);
    retranslate();
    drawIcons();
    setFormatIndex(formatIndex(NumberType::Double));
}

int Inspector::formatIndex(NumberType type) const {
    for (std::size_t i = 0; i < formats_.size(); ++i)
        if (formats_[i].type == type) return static_cast<int>(i);
    return formatIndex(NumberType::Double);
}

void Inspector::setFormatIndex(int index) { format_->setCurrentIndex(index); }

const FloatFormatInfo& Inspector::format() const { return formats_[static_cast<std::size_t>(qMax(0, format_->currentIndex()))]; }

void Inspector::setDecimal(const QString& text) { decimal_->setPlainText(text); }  // textChanged converts it

void Inspector::loadBits(int index, const QString& hex) {
    updating_ = true;
    format_->setCurrentIndex(index);  // without converting: the bits come next
    updating_ = false;
    recolour();
    const FloatInspection inspection = inspectBits(format(), hex.toStdString(), 16);
    if (!inspection.error) display(inspection, nullptr);
}

// A new format: the decimal is converted again; a value written as a power of two (or none) cannot be reread, so the
// bits are read again in the new format instead.
void Inspector::formatChanged() {
    const QString text = decimal_->toPlainText().trimmed();
    if (!text.isEmpty() && !text.contains(QStringLiteral("2^"))) {
        convertDecimal();
        return;
    }
    const QString hex = hex_->toPlainText().trimmed();
    if (hex.isEmpty()) return;
    const FloatInspection inspection = inspectBits(format(), hex.toStdString(), 16);
    if (inspection.error) {
        convertBits(hex_, 16);  // says why
        return;
    }
    display(inspection, nullptr);  // every field in the new format
    message_->setText(tr("The bits were read again in the new format."));
}

void Inspector::recolour() {
    updating_ = true;  // a new layout of colours is not an edit
    static_cast<BitsHighlighter*>(highlighter_)->setLayout(format().exponentBits, dark(this));
    updating_ = false;
}

void Inspector::convertDecimal() {
    const QString text = decimal_->toPlainText().trimmed();
    if (text.isEmpty()) {
        clearOutputs(decimal_);
        return;
    }
    const FloatInspection inspection = inspectDecimal(format(), text.toStdString());
    if (inspection.error) {
        message_->setText(tr("Not a decimal number"));
        strip_->setEnabled(false);
        return;
    }
    display(inspection, decimal_);
}

void Inspector::convertBits(QPlainTextEdit* field, int base) {
    const QString text = field->toPlainText().trimmed();
    if (text.isEmpty()) {
        clearOutputs(field);
        return;
    }
    const FloatInspection inspection = inspectBits(format(), text.toStdString(), base);
    if (inspection.error) {
        message_->setText(inspection.error->code == ErrorCode::LiteralOutOfRange
                              ? tr("%1 has %2 bits").arg(QString::fromStdString(format().name)).arg(format().storageBits)
                          : base == 2 ? tr("Not a binary number")
                                      : tr("Not a hexadecimal number"));
        strip_->setEnabled(false);
        return;
    }
    display(inspection, field);
}

void Inspector::clearOutputs(QWidget* typedIn) {
    updating_ = true;
    for (QPlainTextEdit* field : {decimal_, binary_, hex_})
        if (field != typedIn) field->clear();
    updating_ = false;
    message_->clear();
    for (auto& [key, label] : values_) label->clear();
    current_ = {};
    strip_->setPattern(QString(), QString(), QString());
    down_->setEnabled(false);
    up_->setEnabled(false);
}

void Inspector::display(const FloatInspection& inspection, QWidget* typedIn) {
    current_ = inspection;
    strip_->setPattern(QString::fromStdString(inspection.stored.sign), QString::fromStdString(inspection.stored.exponent),
                       QString::fromStdString(inspection.stored.fraction));
    strip_->setEnabled(true);
    down_->setEnabled(inspection.hasNeighbours);
    up_->setEnabled(inspection.hasNeighbours);
    const FloatBits& b = inspection.stored;
    updating_ = true;
    if (typedIn != binary_)
        binary_->setPlainText(QString::fromStdString(b.sign) + QStringLiteral("  ") + groupsOf4(QString::fromStdString(b.exponent))
                              + QStringLiteral("  ") + groupsOf4(QString::fromStdString(b.fraction)));
    if (typedIn != hex_) hex_->setPlainText(groupsOf4(QString::fromStdString(b.hex)));
    if (typedIn != decimal_) decimal_->setPlainText(view::decimalText(b));
    updating_ = false;
    message_->setText(inspection.note.empty() ? QString() : QCoreApplication::translate("view", inspection.note.c_str()));
    QString kind = view::floatClassName(b.valueClass);
    if (!b.note.empty()) kind += QStringLiteral(" · ") + QCoreApplication::translate("view", b.note.c_str());
    values_["class"]->setText(kind);
    const bool parts = b.valueClass == FloatClass::Normal || b.valueClass == FloatClass::Subnormal;
    const QString exponent = b.exponent2 < 0 ? QString(QChar(0x2212)) + QString::number(-b.exponent2) : QString::number(b.exponent2);
    values_["parts"]->setText(parts ? tr("sign %1 · exponent %2 (field %3) · significand %4")
                                          .arg(b.negative ? QString(QChar(0x2212)) : QStringLiteral("+"), exponent,
                                               QString::number(b.biasedExponent), view::exactDecimal(b.significand))
                                    : QString());
    values_["value"]->setText(view::exactNumber(b));
    const Digits& e = inspection.conversionError;
    values_["error"]->setText(e.digits.empty() ? QString()
                                               : (e.negative ? QString(QChar(0x2212)) : QStringLiteral("+"))
                                                     + view::exactDecimal(Digits{false, e.digits, e.exponent10}));
    const bool finite = parts || b.valueClass == FloatClass::Zero;
    values_["ulp"]->setText(!finite ? QString()
                            : inspection.ulp.digits.empty() ? power(inspection.ulpExponent)
                                                            : power(inspection.ulpExponent) + QStringLiteral(" = ") + view::exactDecimal(inspection.ulp));
    values_["below"]->setText(inspection.hasNeighbours ? view::exactNumber(inspection.below) : QString());
    values_["above"]->setText(inspection.hasNeighbours ? view::exactNumber(inspection.above) : QString());
}

void Inspector::drawIcons() {  // drawn, so the theme's ink colours them (no emoji)
    keysToggle_->setIcon(icons::drawn(icons::Kind::Keyboard, palette().color(QPalette::ButtonText), fontMetrics().height()));
}

void Inspector::retranslate() {
    const std::vector<TypeInfo> types = numberTypes();
    for (std::size_t i = 0; i < formats_.size(); ++i) {
        QString type = tr("display only");
        if (formats_[i].type)
            for (const TypeInfo& t : types)
                if (t.type == *formats_[i].type) type = view::shortTypeName(t);
        format_->setItemText(static_cast<int>(i), QString::fromStdString(formats_[i].name) + QStringLiteral(" · ") + type);
    }
    keysToggle_->setToolTip(tr("Show or hide the keypad"));
    keysToggle_->setAccessibleName(keysToggle_->toolTip());
    for (auto& [key, label] : values_) static_cast<WrapLabel*>(label)->copyAction()->setText(tr("Copy"));
    captions_["format"]->setText(tr("Format"));
    strip_->setAccessibleName(tr("Bits (click one to flip it)"));
    captions_["decimal"]->setText(tr("Decimal"));
    captions_["binary"]->setText(tr("Binary"));
    captions_["hex"]->setText(tr("Hexadecimal"));
    captions_["class"]->setText(tr("Class"));
    captions_["parts"]->setText(tr("Fields"));
    captions_["value"]->setText(tr("Value"));
    captions_["error"]->setText(tr("Conversion error (stored − typed)"));
    captions_["ulp"]->setText(tr("ulp"));
    captions_["below"]->setText(tr("Next below"));
    captions_["above"]->setText(tr("Next above"));
}

bool Inspector::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FocusIn) lastField_ = static_cast<QPlainTextEdit*>(watched);  // where the keypad types
    return QWidget::eventFilter(watched, event);
}

void Inspector::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    if (event->type() == QEvent::PaletteChange && highlighter_) {
        recolour();
        drawIcons();
    }
    QWidget::changeEvent(event);
}
