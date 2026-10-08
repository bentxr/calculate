#include "inspector.hpp"

#include "presenter.hpp"

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxHighlighter>
#include <QVBoxLayout>

using namespace calculate_core;

namespace {

// A field that wraps anywhere, about three lines tall, plain text only.
class FieldEdit : public QPlainTextEdit {
public:
    explicit FieldEdit(QWidget* parent) : QPlainTextEdit(parent) {
        setWordWrapMode(QTextOption::WrapAnywhere);
        setTabChangesFocus(true);
        setFixedHeight(fontMetrics().lineSpacing() * 3 + 2 * frameWidth() + 8);
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

Inspector::Inspector(QWidget* parent) : QWidget(parent), formats_(floatFormats()) {
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    format_ = new QComboBox(this);
    format_->setObjectName("inspectorFormat");
    format_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    format_->setMinimumContentsLength(10);
    for (std::size_t i = 0; i < formats_.size(); ++i) format_->addItem(QString());
    decimal_ = new FieldEdit(this);
    decimal_->setObjectName("inspectorDecimal");
    binary_ = new FieldEdit(this);
    binary_->setObjectName("inspectorBinary");
    hex_ = new FieldEdit(this);
    hex_->setObjectName("inspectorHex");
    for (const char* key : {"format", "decimal", "binary", "hex"}) {
        auto* caption = new QLabel(this);
        caption->setObjectName(QStringLiteral("inspectorLabel:") + key);
        captions_[key] = caption;
    }
    form->addRow(captions_["format"], format_);
    form->addRow(captions_["decimal"], decimal_);
    form->addRow(captions_["binary"], binary_);
    form->addRow(captions_["hex"], hex_);
    layout->addLayout(form);
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
        auto* value = new QLabel(this);
        value->setObjectName(QStringLiteral("inspector:") + key);
        value->setWordWrap(true);
        value->setTextFormat(Qt::PlainText);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
    grid->addWidget(down_, row - 2, 2, Qt::AlignTop);
    grid->addWidget(up_, row - 1, 2, Qt::AlignTop);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addStretch();
    highlighter_ = new BitsHighlighter(binary_->document(), 11, dark(this));
    connect(decimal_, &QPlainTextEdit::textChanged, this, [this] {
        if (!updating_) convertDecimal();
    });
    connect(format_, &QComboBox::currentIndexChanged, this, [this] {
        static_cast<BitsHighlighter*>(highlighter_)->setLayout(format().exponentBits, dark(this));
        convertDecimal();
    });
    retranslate();
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

void Inspector::convertDecimal() {
    const QString text = decimal_->toPlainText().trimmed();
    if (text.isEmpty()) {
        clearOutputs();
        return;
    }
    const FloatInspection inspection = inspectDecimal(format(), text.toStdString());
    if (inspection.error) {
        message_->setText(view::errorText(*inspection.error, text));
        return;
    }
    show(inspection, decimal_);
}

void Inspector::clearOutputs() {
    updating_ = true;
    binary_->clear();
    hex_->clear();
    updating_ = false;
    message_->clear();
    for (auto& [key, label] : values_) label->clear();
}

void Inspector::show(const FloatInspection& inspection, QWidget* typedIn) {
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

void Inspector::retranslate() {
    const std::vector<TypeInfo> types = numberTypes();
    for (std::size_t i = 0; i < formats_.size(); ++i) {
        QString type = tr("display only");
        if (formats_[i].type)
            for (const TypeInfo& t : types)
                if (t.type == *formats_[i].type) type = view::shortTypeName(t);
        format_->setItemText(static_cast<int>(i), QString::fromStdString(formats_[i].name) + QStringLiteral(" · ") + type);
    }
    captions_["format"]->setText(tr("Format"));
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

void Inspector::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    if (event->type() == QEvent::PaletteChange && highlighter_)
        static_cast<BitsHighlighter*>(highlighter_)->setLayout(format().exponentBits, dark(this));
    QWidget::changeEvent(event);
}
