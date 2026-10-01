#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QList>
#include <QString>

// Pure translation from the engine's results to display text; the numeric logic stays in the engine.
namespace view {

// A floating value: the digits the bound guarantees, the noise digits after them, and the power of
// ten when the value is written as d.ddd × 10^exponent. Nothing is truncated.
struct ValueParts {
    QString trusted;   // the sign and the trusted digits ("−1.000000000000000")
    QString noise;     // the digits beyond them; empty when every digit is trusted
    QString exponent;  // "30", "−7", or empty for positional values
};

// An exact result: the reduced fraction, and its decimal with the recurring block apart.
struct FractionParts {
    QString sign;         // "−" or empty
    QString numerator;
    QString denominator;  // "1" for whole numbers
    QString decimal;      // "0.", "1.75"…; empty for whole numbers and for periods too long to show
    QString recurring;    // the repeating block, drawn overlined
};

// One row of the Details card; `key` names it for its explanation().
struct DetailRow {
    QString key;
    QString label;
    QString value;
};

QString typeLabel(const calculate_core::TypeInfo& type);
QString shortTypeName(const calculate_core::TypeInfo& type);
QString shortTypeNameSource(const calculate_core::TypeInfo& type);  // its English source text
QString typeDigits(const calculate_core::TypeInfo& type);  // "~16 significant digits"
QString typeDetail(const calculate_core::TypeInfo& type);  // the C++ type and its bits
ValueParts valueParts(const calculate_core::Result& result);
FractionParts fractionParts(const calculate_core::Result& result);
QString verdict(const QString& conditionNumber);
QList<DetailRow> details(const calculate_core::Result& result, const calculate_core::TypeInfo& type);
QString explanation(const QString& key);
QString errorText(const calculate_core::Error& error, const QString& expression);
QString statisticsExpression(const QString& function, const QString& values);

}  // namespace view
