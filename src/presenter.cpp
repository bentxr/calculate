#include "presenter.hpp"

#include "settings.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace view {

using namespace calculate_core;

namespace {

QString fromStd(const std::string& s) { return QString::fromStdString(s); }

QString minus() { return QString(QChar(0x2212)); }

// A number for display, with the decimal separator chosen in the settings.
QString decimal(QString number) { return settings::decimalComma() ? number.replace('.', ',') : number; }
QString number(const std::string& s) { return decimal(fromStd(s)); }

// Laid out like the CLI does: positional for −7 <= exponent < 21, d.ddd × 10^exponent otherwise.
ValueParts split(const Digits& value, int trustedDigits) {
    const QString sig = fromStd(value.digits);
    const int n = static_cast<int>(sig.size());
    const long long e = value.exponent10;
    const int t = qMin(trustedDigits, n);
    ValueParts p;
    QString* out = &p.trusted;
    if (value.negative) p.trusted += minus();
    auto digit = [&](int i) {
        if (i == t) out = &p.noise;
        *out += sig[i];
    };
    if (e >= -7 && e < 21) {
        if (e < 0) {
            p.trusted += QStringLiteral("0.") + QString(static_cast<int>(-e - 1), QChar('0'));
            for (int i = 0; i < n; ++i) digit(i);
        } else {
            for (long long i = 0; i <= e; ++i) {
                if (i < n) digit(static_cast<int>(i));
                else *out += QChar('0');
            }
            if (n > e + 1) {
                *out += QChar('.');
                for (int i = static_cast<int>(e) + 1; i < n; ++i) digit(i);
            }
        }
    } else {
        digit(0);
        if (n > 1) {
            *out += QChar('.');
            for (int i = 1; i < n; ++i) digit(i);
        }
        p.exponent = e < 0 ? minus() + QString::number(-e) : QString::number(e);
    }
    return p;
}

// The decimal exponent of a formatted magnitude such as "4.4e-17"; "0" is very small, "inf" very large.
int exponentOf(const QString& s) {
    if (s == "inf") return 1 << 30;
    const int e = s.indexOf('e');
    return e < 0 ? -(1 << 30) : s.mid(e + 1).toInt();
}

QString translatedNote(const std::string& note) {
    if (note == "same format as binary128 here") return QCoreApplication::translate("view", "same format as binary128 here");
    if (note == "identical to double here") return QCoreApplication::translate("view", "identical to double here");
    if (note == "software, no subnormals") return {};  // left out: people read it as noise
    return fromStd(note);
}

QString translatedLabel(const std::string& label) {
    if (label == "Single") return QCoreApplication::translate("view", "Single");
    if (label == "Double") return QCoreApplication::translate("view", "Double");
    if (label == "Extended") return QCoreApplication::translate("view", "Extended");
    if (label == "Exact") return QCoreApplication::translate("view", "Exact");
    if (label == "Quadruple") return QCoreApplication::translate("view", "Quadruple");
    if (label == "Octuple") return QCoreApplication::translate("view", "Octuple");
    if (label == "Binary512") return QCoreApplication::translate("view", "Hexadecuple");  // 16 × single
    return fromStd(label);
}

}  // namespace

QString typeLabel(const TypeInfo& t) {
    const QString dot = QStringLiteral(" · ");
    QString s = translatedLabel(t.label) + dot + fromStd(t.cppName);
    if (t.type == NumberType::Exact) return s + dot + QCoreApplication::translate("view", "no rounding");
    s += dot + QCoreApplication::translate("view", "%1-bit").arg(t.storageBits) + dot + QCoreApplication::translate("view", "~%1 digits").arg(t.decimalDigits);
    if (!translatedNote(t.note).isEmpty()) s += dot + translatedNote(t.note);
    return s;
}

QString shortTypeName(const TypeInfo& t) { return translatedLabel(t.label); }
QString shortTypeNameSource(const TypeInfo& t) { return t.label == "Binary512" ? QStringLiteral("Hexadecuple") : fromStd(t.label); }

QString typeDigits(const TypeInfo& t) {
    if (t.type == NumberType::Exact) return QCoreApplication::translate("view", "every digit exact");
    return QCoreApplication::translate("view", "~%1 significant digits").arg(t.decimalDigits);
}

QString typeDetail(const TypeInfo& t) {
    if (t.type == NumberType::Exact) return QCoreApplication::translate("view", "%1 · fractions, no rounding").arg(fromStd(t.cppName));
    QString s = QCoreApplication::translate("view", "%1 · %2-bit storage · %3-bit significand")
                    .arg(fromStd(t.cppName)).arg(t.storageBits).arg(t.precisionBits);
    if (!translatedNote(t.note).isEmpty()) s += QStringLiteral(" · ") + translatedNote(t.note);
    return s;
}

ValueParts valueParts(const Result& r) {
    if (r.error || r.exact || r.commentOnly) return {};
    return split(r.value, r.trustedDigits);
}

FractionParts fractionParts(const Result& r) {
    if (!r.exact) return {};
    const Fraction& f = *r.exact;
    FractionParts p;
    if (f.negative) p.sign = minus();
    p.numerator = fromStd(f.numerator);
    p.denominator = fromStd(f.denominator);
    if (f.hasDecimal && f.denominator != "1") {
        p.decimal = fromStd(f.integerPart) + "." + fromStd(f.fractionDigits);
        p.recurring = fromStd(f.repeatingDigits);
    }
    return p;
}

QString verdict(const QString& conditionNumber) {
    const int e = exponentOf(conditionNumber);
    if (e <= 0) return QCoreApplication::translate("view", "well-conditioned");
    if (e <= 7) return QCoreApplication::translate("view", "moderately conditioned");
    return QCoreApplication::translate("view", "ill-conditioned: no algorithm can do better in this type");
}

QString conversionText(const Result& r) {
    if (!r.conversion) return {};
    const QString text = fromStd(r.conversion->text);
    return settings::decimalComma() ? withDecimalComma(text) : text;
}

QString valueText(const Result& r) {
    const bool comma = settings::decimalComma();
    if (r.exact) return oneLine(comma ? withDecimalComma(fractionParts(r)) : fractionParts(r));
    return oneLine(comma ? withDecimalComma(valueParts(r)) : valueParts(r));
}

QList<DetailRow> details(const Result& r, const TypeInfo& t) {
    if (r.error || r.commentOnly) return {};  // a note has no value
    // A conversion takes the screen: the value as computed leads the card, then the form it is shown in.
    QList<DetailRow> converted;
    if (r.conversion)
        converted = {{"value", QCoreApplication::translate("view", "Value"), valueText(r)},
                     {"conversion", QCoreApplication::translate("view", "Shown as"), fromStd(r.conversion->target)}};
    const QString expression = settings::decimalComma() ? withDecimalComma(fromStd(r.expression)) : fromStd(r.expression);
    const DetailRow evaluated{"evaluated", QCoreApplication::translate("view", "Evaluated"), expression};
    if (r.exact)
        return converted
               + QList<DetailRow>{{"exact", QCoreApplication::translate("view", "Error"), QCoreApplication::translate("view", "exact · no rounding error")},
                                  {"type", QCoreApplication::translate("view", "Number type"),
                                   QCoreApplication::translate("view", "%1, exact fractions").arg(fromStd(t.cppName))},
                                  evaluated};
    QString measured = r.measuredAvailable ? number(r.measured) : QCoreApplication::translate("view", "unavailable");
    if (r.measuredAvailable && !r.measurementReliable) measured += QStringLiteral(" (") + QCoreApplication::translate("view", "unreliable") + QStringLiteral(")");
    QList<DetailRow> rows{
        {"bound", QCoreApplication::translate("view", "Guaranteed bound"), number(r.bound)},
        {"measured", QCoreApplication::translate("view", "Measured error"), measured},
        {"trusted", QCoreApplication::translate("view", "Trusted digits"),
         QCoreApplication::translate("view", "%1 by the bound, %2 by the measurement").arg(r.trustedDigits).arg(r.trustedDigitsMeasured)},
        {"condition", QCoreApplication::translate("view", "Condition number κ"), number(r.conditionNumber) + QStringLiteral(" · ") + verdict(fromStd(r.conditionNumber))},
        {"input", QCoreApplication::translate("view", "Input error"), number(r.inputError)},
        {"rounding", QCoreApplication::translate("view", "Rounding error"), number(r.roundingError)},
        {"library", QCoreApplication::translate("view", "Library error"), number(r.libraryError)},
        {"operations", QCoreApplication::translate("view", "Rounded operations"), QString::number(r.roundingOperations)},
        {"type", QCoreApplication::translate("view", "Number type"),
         QCoreApplication::translate("view", "%1, %2-bit significand").arg(fromStd(t.cppName)).arg(t.precisionBits)},
    };
    if (!r.boundComplete)
        rows.append({"incomplete", QCoreApplication::translate("view", "Incomplete"),
                     QCoreApplication::translate("view", "an uncertain argument was accepted")});
    rows.append(evaluated);
    return converted + rows;
}

QString explanation(const QString& key) {
    if (key == "value")
        return QCoreApplication::translate("view", "The result as computed in this number type; the screen shows it converted.");
    if (key == "conversion")
        return QCoreApplication::translate("view", "The form you asked for with “to”: the same value, written another way.");
    if (key == "bound")
        return QCoreApplication::translate("view", "A proven upper limit on how far the shown value can be from the exact result.");
    if (key == "measured")
        return QCoreApplication::translate("view", "The actual difference from the same calculation redone with far more precision. "
                                                   "An estimate, usually much smaller than the guaranteed bound.");
    if (key == "trusted")
        return QCoreApplication::translate("view", "How many leading digits you can rely on: according to the guaranteed bound, "
                                                   "and according to the measured error.");
    if (key == "condition")
        return QCoreApplication::translate("view", "How much the problem itself magnifies small changes in its input. "
                                                   "When it is large, no algorithm can do better in this number type.");
    if (key == "input")
        return QCoreApplication::translate("view", "The error of storing the numbers you typed in this type: "
                                                   "0.1, for example, has no exact binary form.");
    if (key == "rounding")
        return QCoreApplication::translate("view", "The error added by rounding the result of each arithmetic operation.");
    if (key == "library")
        return QCoreApplication::translate("view", "The error of functions such as sin or exp, which can't be computed exactly; "
                                                   "it is bounded by their tested accuracy.");
    if (key == "operations")
        return QCoreApplication::translate("view", "How many operations had to round their result.");
    if (key == "type")
        return QCoreApplication::translate("view", "The C++ type the calculation ran in, and the bits of its significand: "
                                                   "more bits, more correct digits.");
    if (key == "evaluated")
        return QCoreApplication::translate("view", "The expression as it was computed, with Ans and M replaced by what they stand for.");
    if (key == "exact")
        return QCoreApplication::translate("view", "Exact arithmetic works with fractions of whole numbers, so nothing is ever rounded: "
                                                   "the result is exactly right.");
    if (key == "incomplete")
        return QCoreApplication::translate("view", "You chose to proceed with an uncertain argument for a whole-number function; "
                                                   "that uncertainty is not included in the figures above.");
    return {};
}

QString copyText(const Result& r, CopyForm form, const TypeInfo& t) {
    if (r.error) return {};
    QString value;
    if (r.exact) {
        const Fraction& f = *r.exact;
        value = QString(f.negative ? "-" : "") + fromStd(f.numerator);
        if (f.denominator != "1") value += "/" + fromStd(f.denominator);
        if (form != CopyForm::Details) return value;  // exact: every digit trusted, and no bound
    } else {
        const ValueParts p = split(r.value, r.trustedDigits);
        const QString exponent = p.exponent.isEmpty() ? QString() : "e" + QString(p.exponent).replace(minus(), "-");
        if (form == CopyForm::Trusted) {
            if (qMin(r.trustedDigits, static_cast<int>(r.value.digits.size())) == 0) return {};
            QString trusted = p.trusted;
            if (trusted.endsWith('.')) trusted.chop(1);
            return decimal(trusted.replace(minus(), "-")) + exponent;
        }
        value = decimal(QString(p.trusted + p.noise).replace(minus(), "-")) + exponent;
        if (form == CopyForm::Value) return value;
        if (form == CopyForm::ValueAndBound) return value + QStringLiteral(" ± ") + number(r.bound);
    }
    const QString expression = settings::decimalComma() ? withDecimalComma(fromStd(r.expression)) : fromStd(r.expression);
    QStringList lines{expression + QStringLiteral(" = ") + value};
    for (const DetailRow& row : details(r, t))
        if (row.key != "evaluated") lines << row.label + QStringLiteral(": ") + row.value;
    return lines.join('\n');
}

ValueParts withDecimalComma(ValueParts p) {
    p.trusted.replace('.', ',');
    p.noise.replace('.', ',');
    return p;
}

FractionParts withDecimalComma(FractionParts p) {
    p.decimal.replace('.', ',');
    return p;
}

QString withDecimalComma(const QString& expression) {
    QString s = expression;
    s.replace(',', ';');  // in the engine's text a comma only ever separates arguments
    return s.replace('.', ',');
}

bool incomplete(const Error& error) {
    return error.code == ErrorCode::UnexpectedEnd || error.code == ErrorCode::MissingClosingParenthesis;
}

QString warningText(const Warning& w, const QString& expression) {
    QString part = QString::fromUtf8(expression.toUtf8().mid(static_cast<int>(w.begin), static_cast<int>(w.end - w.begin)));
    if (settings::decimalComma()) part = withDecimalComma(part);  // quoted as the screen shows it
    switch (w.code) {
    case WarningCode::EmptyRange: return QCoreApplication::translate("view", "%1 has no terms").arg(part);
    }
    return {};
}

QString errorText(const Error& e, const QString& expression) {
    const QByteArray bytes = expression.toUtf8();
    QString part = QString::fromUtf8(bytes.mid(static_cast<int>(e.begin), static_cast<int>(e.end - e.begin)));
    if (settings::decimalComma()) part = withDecimalComma(part);  // quoted as the screen shows it
    const QString name = part.section('(', 0, 0).trimmed();
    switch (e.code) {
    case ErrorCode::InvalidCharacter: return QCoreApplication::translate("view", "Unexpected character “%1”").arg(part);
    case ErrorCode::InvalidNumber: return QCoreApplication::translate("view", "“%1” is not a valid number").arg(part);
    case ErrorCode::UnexpectedToken: return QCoreApplication::translate("view", "Unexpected “%1”").arg(part);
    case ErrorCode::UnexpectedEnd: return QCoreApplication::translate("view", "The expression ends too early");
    case ErrorCode::MissingClosingParenthesis: return QCoreApplication::translate("view", "Missing “)”");
    case ErrorCode::MissingOperator: return QCoreApplication::translate("view", "Missing operator before “%1” (write 2×π, not 2π)").arg(part);
    case ErrorCode::UnknownName:
        if (part == "Ans") return QCoreApplication::translate("view", "There is no previous result yet");
        if (part == "M") return QCoreApplication::translate("view", "The memory is empty");
        return QCoreApplication::translate("view", "Unknown name “%1”").arg(name);
    case ErrorCode::WrongArgumentCount: return QCoreApplication::translate("view", "Wrong number of arguments for %1").arg(name);
    case ErrorCode::NotAvailableInExact:
        return QCoreApplication::translate("view", "Exact arithmetic cannot represent %1: its result is irrational. Switch to a floating type to compute it.").arg(name);
    case ErrorCode::LiteralOutOfRange: return QCoreApplication::translate("view", "%1 cannot be represented in this number type").arg(part);
    case ErrorCode::DivisionByZero: return QCoreApplication::translate("view", "Division by zero");
    case ErrorCode::DomainError: return QCoreApplication::translate("view", "%1 is not defined here").arg(part);
    case ErrorCode::Overflow: return QCoreApplication::translate("view", "The result is too large for this number type");
    case ErrorCode::IrrationalResult: return QCoreApplication::translate("view", "The exact result of %1 is irrational").arg(part);
    case ErrorCode::ArgumentTooLarge: return QCoreApplication::translate("view", "The argument of %1 is too large to reduce accurately").arg(name);
    case ErrorCode::NotAnInteger: return QCoreApplication::translate("view", "%1 needs whole numbers").arg(part);
    case ErrorCode::UncertainDiscreteArgument:
        return QCoreApplication::translate("view", "%1 needs an exactly known whole number, but its argument carries an error. "
                  "If you proceed anyway, the error report will not include that error.").arg(part);
    case ErrorCode::ArgumentNearJump: return QCoreApplication::translate("view", "%1 jumps within the error of its arguments, so the result could be off by a whole step. If you proceed anyway, the error report will not include that error.").arg(part);
    case ErrorCode::ArgumentNearEdge: return QCoreApplication::translate("view", "The error of the argument of %1 reaches a point where it is not defined or not smooth, so no bound can be given. If you proceed anyway, the error report will not include that.").arg(part);
    case ErrorCode::Cancelled: return QCoreApplication::translate("view", "Cancelled");
    case ErrorCode::UnknownTarget: return QCoreApplication::translate("view", "Unknown conversion “%1”").arg(part);
    case ErrorCode::TooManyTerms: return QCoreApplication::translate("view", "%1 has too many terms").arg(part);
    case ErrorCode::ReservedName: return QCoreApplication::translate("view", "“%1” is a reserved name").arg(part);
    }
    return {};
}

QString statisticsExpression(const QString& function, const QString& values) {
    const QStringList list = values.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    if (list.isEmpty()) return {};
    return function + "(" + list.join(", ") + ")";
}

// What the engine does (parser.cpp, statistic(); functions.hpp, Median), so the user sees the algorithm.
QString algorithm(const QString& function) {
    if (function == "mean")
        return QCoreApplication::translate("view", "The values are added from left to right, then divided by n.");
    if (function == "median")
        return QCoreApplication::translate("view", "The values are sorted; the middle one is taken, or the mean of the two middle ones.");
    if (function == "var")
        return QCoreApplication::translate("view", "Two passes: the mean first, then the squared deviations from it, divided by n − 1 (a sample).");
    if (function == "varp")
        return QCoreApplication::translate("view", "Two passes: the mean first, then the squared deviations from it, divided by n (the whole population).");
    if (function == "stdev")
        return QCoreApplication::translate("view", "The square root of the sample variance (two passes, divided by n − 1).");
    if (function == "stdevp")
        return QCoreApplication::translate("view", "The square root of the population variance (two passes, divided by n).");
    return {};
}

QString oneLine(const ValueParts& parts) {
    QString s = parts.trusted;
    if (!parts.noise.isEmpty()) s += "|" + parts.noise;
    if (!parts.exponent.isEmpty()) s += "×10^" + parts.exponent;
    return s;
}

QString oneLine(const FractionParts& parts) {
    if (parts.denominator == "1") return parts.sign + parts.numerator;
    QString s = parts.sign + parts.numerator + "/" + parts.denominator;
    if (!parts.decimal.isEmpty()) s += " = " + parts.sign + parts.decimal;
    if (!parts.recurring.isEmpty()) s += "(" + parts.recurring + ")";
    return s;
}

// Written out in full, never with %, so the answers don't depend on how % is read.
QList<PercentageRow> percentageRows(const QString& first, const QString& second) {
    if (first.isEmpty() || second.isEmpty()) return {};
    const QString a = "(" + first + ")", b = "(" + second + ")";
    return {
        {"change", QCoreApplication::translate("view", "Change from 1 to 2 (%)"), "(" + b + "−" + a + ")÷" + a + "×100"},
        {"changeBack", QCoreApplication::translate("view", "Change from 2 to 1 (%)"), "(" + a + "−" + b + ")÷" + b + "×100"},
        {"secondOfFirst", QCoreApplication::translate("view", "2 as a percentage of 1"), b + "÷" + a + "×100"},
        {"firstOfSecond", QCoreApplication::translate("view", "1 as a percentage of 2"), a + "÷" + b + "×100"},
        {"plus", QCoreApplication::translate("view", "1 plus 2 %"), a + "+" + a + "×" + b + "÷100"},
        {"minus", QCoreApplication::translate("view", "1 minus 2 %"), a + "−" + a + "×" + b + "÷100"},
        {"of", QCoreApplication::translate("view", "2 % of 1"), a + "×" + b + "÷100"},
    };
}

}  // namespace view
