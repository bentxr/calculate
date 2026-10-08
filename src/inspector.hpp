#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QWidget>

#include <map>
#include <vector>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSyntaxHighlighter;

// The bits of a pattern, coloured by field and grouped by four; a click (or tap) on a bit flips it.
class BitStrip : public QWidget {
    Q_OBJECT

public:
    explicit BitStrip(QWidget* parent = nullptr);
    void setPattern(const QString& sign, const QString& exponent, const QString& fraction);  // "0"/"1" digits
    int bitCount() const { return static_cast<int>(bits_.size()); }
    QPoint bitCenter(int index) const;  // 0 = the most significant bit
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;

signals:
    void flipped(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // Each bit's box at this width: four bits a group, a wider gap between fields, wrapping to the next line.
    std::vector<QRect> layOut(int width) const;

    QString bits_;
    int exponentBits_ = 0;
    std::vector<QRect> boxes_;
};

// The IEEE 754 tool: a decimal, its bits in binary and hexadecimal, and what the stored value is
// exactly, for every format of floatFormats(). Editing any field updates the others.
class Inspector : public QWidget {
    Q_OBJECT

public:
    explicit Inspector(QWidget* parent = nullptr);

    int formatIndex(calculate_core::NumberType type) const;  // the list row of a type's own format
    void setFormatIndex(int index);
    void setDecimal(const QString& text);                    // as if typed in the decimal field
    void loadBits(int formatIndex, const QString& hex);      // a stored pattern, e.g. a result's or a neighbour
    void retranslate();

protected:
    void changeEvent(QEvent* event) override;  // LanguageChange → retranslate; PaletteChange → recolour

private:
    const calculate_core::FloatFormatInfo& format() const;
    void convertDecimal();
    void convertBits(QPlainTextEdit* field, int base);  // base 2 or 16
    // Fills every field and output but `typedIn`, which keeps what the user wrote.
    void display(const calculate_core::FloatInspection& inspection, QWidget* typedIn);
    void clearOutputs(QWidget* typedIn);
    void formatChanged();
    void recolour();  // the binary field's colours, for the format and the theme  // an emptied field: the others and the outputs too

    std::vector<calculate_core::FloatFormatInfo> formats_;
    QComboBox* format_ = nullptr;
    QPlainTextEdit* decimal_ = nullptr;
    QPlainTextEdit* binary_ = nullptr;
    QPlainTextEdit* hex_ = nullptr;
    BitStrip* strip_ = nullptr;
    QSyntaxHighlighter* highlighter_ = nullptr;
    QLabel* message_ = nullptr;
    std::map<QString, QLabel*> captions_;  // by key: the bold caption of each output row
    std::map<QString, QLabel*> values_;    // and its value
    QPushButton* down_ = nullptr;
    QPushButton* up_ = nullptr;
    bool updating_ = false;  // filling the fields: their change signals are not edits
    calculate_core::FloatInspection current_;  // what the fields show now: its neighbours for ◄ ►
};
