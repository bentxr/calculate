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

// The IEEE 754 tool: a decimal, its bits in binary and hexadecimal, and what the stored value is
// exactly, for every format of floatFormats(). Editing any field updates the others.
class Inspector : public QWidget {
    Q_OBJECT

public:
    explicit Inspector(QWidget* parent = nullptr);

    int formatIndex(calculate_core::NumberType type) const;  // the list row of a type's own format
    void setFormatIndex(int index);
    void setDecimal(const QString& text);                    // as if typed in the decimal field
    void retranslate();

protected:
    void changeEvent(QEvent* event) override;  // LanguageChange → retranslate; PaletteChange → recolour

private:
    const calculate_core::FloatFormatInfo& format() const;
    void convertDecimal();
    // Fills every field and output but `typedIn`, which keeps what the user wrote.
    void show(const calculate_core::FloatInspection& inspection, QWidget* typedIn);
    void clearOutputs();

    std::vector<calculate_core::FloatFormatInfo> formats_;
    QComboBox* format_ = nullptr;
    QPlainTextEdit* decimal_ = nullptr;
    QPlainTextEdit* binary_ = nullptr;
    QPlainTextEdit* hex_ = nullptr;
    QSyntaxHighlighter* highlighter_ = nullptr;
    QLabel* message_ = nullptr;
    std::map<QString, QLabel*> captions_;  // by key: the bold caption of each output row
    std::map<QString, QLabel*> values_;    // and its value
    QPushButton* down_ = nullptr;
    QPushButton* up_ = nullptr;
    bool updating_ = false;  // filling the fields: their change signals are not edits
};
