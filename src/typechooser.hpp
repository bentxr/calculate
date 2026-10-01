#pragma once

#include <QComboBox>

// The number-type menu. Closed, it shows only the short name ("Double"); open, it lists every type
// with what it means: its significant digits, a bar that compares them, and its C++ type and bits.
class TypeChooser : public QComboBox {
    Q_OBJECT

public:
    enum Role { DigitsRole = Qt::UserRole, DetailRole, PrecisionRole };

    explicit TypeChooser(QWidget* parent = nullptr);
    void retranslate();  // sets every text in the current language
};
