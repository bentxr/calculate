#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QComboBox>

// The number-type menu. Closed, it shows only the short name ("Double"); open, it lists every type
// with what it means: its significant digits, a bar that compares them, and its C++ type and bits.
class TypeChooser : public QComboBox {
    Q_OBJECT

public:
    enum Role { DigitsRole = Qt::UserRole, DetailRole, PrecisionRole, TypeRole };

    explicit TypeChooser(QWidget* parent = nullptr);
    calculate_core::NumberType currentType() const;
    void setCurrentType(calculate_core::NumberType type);
    void retranslate();  // sets every text in the current language
};
