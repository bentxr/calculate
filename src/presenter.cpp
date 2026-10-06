#include "presenter.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace view {

using namespace calculate_core;

namespace {

QString fromStd(const std::string& s) { return QString::fromStdString(s); }

QString minus() { return QString(QChar(0x2212)); }

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
    if (r.error || r.exact) return {};
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

QList<DetailRow> details(const Result& r, const TypeInfo& t) {
    if (r.error) return {};
    const DetailRow evaluated{"evaluated", QCoreApplication::translate("view", "Evaluated"), fromStd(r.expression)};
    if (r.exact)
        return {{"exact", QCoreApplication::translate("view", "Error"), QCoreApplication::translate("view", "exact · no rounding error")},
                {"type", QCoreApplication::translate("view", "Number type"),
                 QCoreApplication::translate("view", "%1, exact fractions").arg(fromStd(t.cppName))},
                evaluated};
    QString measured = r.measuredAvailable ? fromStd(r.measured) : QCoreApplication::translate("view", "unavailable");
    if (r.measuredAvailable && !r.measurementReliable) measured += QStringLiteral(" (") + QCoreApplication::translate("view", "unreliable") + QStringLiteral(")");
    QList<DetailRow> rows{
        {"bound", QCoreApplication::translate("view", "Guaranteed bound"), fromStd(r.bound)},
        {"measured", QCoreApplication::translate("view", "Measured error"), measured},
        {"trusted", QCoreApplication::translate("view", "Trusted digits"),
         QCoreApplication::translate("view", "%1 by the bound, %2 by the measurement").arg(r.trustedDigits).arg(r.trustedDigitsMeasured)},
        {"condition", QCoreApplication::translate("view", "Condition number κ"), fromStd(r.conditionNumber) + QStringLiteral(" · ") + verdict(fromStd(r.conditionNumber))},
        {"input", QCoreApplication::translate("view", "Input error"), fromStd(r.inputError)},
        {"rounding", QCoreApplication::translate("view", "Rounding error"), fromStd(r.roundingError)},
        {"library", QCoreApplication::translate("view", "Library error"), fromStd(r.libraryError)},
        {"operations", QCoreApplication::translate("view", "Rounded operations"), QString::number(r.roundingOperations)},
        {"type", QCoreApplication::translate("view", "Number type"),
         QCoreApplication::translate("view", "%1, %2-bit significand").arg(fromStd(t.cppName)).arg(t.precisionBits)},
    };
    if (!r.boundComplete)
        rows.append({"incomplete", QCoreApplication::translate("view", "Incomplete"),
                     QCoreApplication::translate("view", "an uncertain argument was accepted")});
    rows.append(evaluated);
    return rows;
}

QString explanation(const QString& key) {
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

QString errorText(const Error& e, const QString& expression) {
    const QByteArray bytes = expression.toUtf8();
    const QString part = QString::fromUtf8(bytes.mid(static_cast<int>(e.begin), static_cast<int>(e.end - e.begin)));
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
    case ErrorCode::Cancelled: return QCoreApplication::translate("view", "Cancelled");
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

}  // namespace view
