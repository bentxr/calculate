#pragma once

#include <calculate-core/calculate-core.hpp>

#include <QList>
#include <QString>

#include <optional>

// Pure translation from the engine's results to display text; the numeric logic stays in the engine.
namespace view {

// A floating value: the digits the bound guarantees, the noise digits after them, and the power of
// ten when the value is written as d.ddd × 10^exponent. Nothing is truncated.
struct ValueParts {
    QString trusted;   // the sign and the trusted digits ("−1.000000000000000")
    QString noise;     // the digits beyond them; empty when every digit is trusted
    QString exponent;  // "30", "−7", or empty for positional values
    QString uncertainty;          // "± U" when the result has uncertain inputs: U's two digits ("2.5", "0.20")
    QString uncertaintyExponent;  // U's power of ten, when it has one ("−15")
};

// An exact result: the reduced fraction, and its decimal with the recurring block apart.
struct FractionParts {
    QString sign;         // "−" or empty
    QString numerator;
    QString denominator;  // "1" for whole numbers
    QString decimal;      // "0.", "1.75"…; empty for whole numbers and for periods too long to show
    QString recurring;    // the repeating block, drawn overlined
    QString uncertainty;          // as in ValueParts
    QString uncertaintyExponent;
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
QString warningText(const calculate_core::Warning& warning, const QString& expression);
// The expression only stops short (an operand or a ")" still to come): no fault yet while it is typed.
bool incomplete(const calculate_core::Error& error);

// The decimal comma, for display (the engine always writes a point): a value's digits, a fraction's decimal, or an
// expression or a part of one (`.` → `,`, and the argument separator `,` → `;`).
ValueParts withDecimalComma(ValueParts parts);
FractionParts withDecimalComma(FractionParts parts);
QString withDecimalComma(const QString& expression);

// A result on one line of plain text, as the screen shows it: "1.000|2×10^−7", "−1/3 = −0.(3)".
QString oneLine(const ValueParts& parts);
QString oneLine(const FractionParts& parts);

QString conversionText(const calculate_core::Result& result);  // "" when there is none
QString offBy(const calculate_core::Result& result);  // how far an approximate conversion is ("3.3e-2"); "" when exact
// A conversion that shows a number, as the screen draws a value (the bar, the noise); none for other conversions.
std::optional<ValueParts> conversionParts(const calculate_core::Result& result);
QString valueText(const calculate_core::Result& result);       // the value on one line, as the screen shows it

enum class CopyForm { Value, Trusted, ValueAndBound, Details };
// The result as plain text for the clipboard: ASCII signs ("-", "e30"), no bar; empty when the form has
// nothing to give (an error, or no trusted digit).
QString copyText(const calculate_core::Result& result, CopyForm form, const calculate_core::TypeInfo& type);
QString statisticsExpression(const QString& function, const QString& values);
// How the engine computes a statistic, in one line; empty for anything else.
QString algorithm(const QString& function);

struct PercentageRow {
    QString key;         // stable id: change, changeBack, secondOfFirst, firstOfSecond, plus, minus, of
    QString title;       // translated: "Change from 1 to 2 (%)"
    QString expression;  // what the engine evaluates
};
// The seven percentage questions about two values (as typed, with the screen's signs), in the order of the tool;
// empty when either value is empty.
QList<PercentageRow> percentageRows(const QString& first, const QString& second);

// The arguments of the function being typed, split around the one the cursor is in: before "nCr(n, ",
// current "r", after ")". Optional arguments in brackets ("log(x[, base])"); a repeated one shown as often as
// reached, then "…". Separators as the decimal-comma mode writes them. All empty for an unknown name.
struct HintParts {
    QString before, current, after;
};
HintParts argumentHint(const QString& name, int argument);

}  // namespace view
