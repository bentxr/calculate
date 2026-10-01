#include "presenter.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace view {

using namespace calculate_core;

namespace {

QString fromStd(const std::string& s) { return QString::fromStdString(s); }

// Trusted digits, noise digits and the exponent suffix, laid out like the CLI does.
struct Parts {
    QString trusted;
    QString noise;
    QString suffix;
};

Parts split(const Digits& value, int trustedDigits) {
    const QString sig = fromStd(value.digits);
    const int n = static_cast<int>(sig.size());
    const long long e = value.exponent10;
    const int t = qMin(trustedDigits, n);
    Parts p;
    QString* out = &p.trusted;
    if (value.negative) p.trusted += QChar(0x2212);
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
        p.suffix = QStringLiteral(" × 10<sup>%1</sup>").arg(e < 0 ? QString(QChar(0x2212)) + QString::number(-e) : QString::number(e));
    }
    return p;
}

QString fractionText(const Fraction& f) {
    const QString sign = f.negative ? QString(QChar(0x2212)) : QString();
    if (f.denominator == "1") return sign + fromStd(f.numerator);
    QString s = sign + fromStd(f.numerator) + "/" + fromStd(f.denominator);
    if (f.hasDecimal) {
        s += " = " + sign + fromStd(f.integerPart) + "." + fromStd(f.fractionDigits);
        if (!f.repeatingDigits.empty()) s += "(" + fromStd(f.repeatingDigits) + ")";
    }
    return s;
}

QString trustedText(const Result& r) {
    const int n = static_cast<int>(r.value.digits.size());
    if (r.trustedDigits >= n) return QCoreApplication::translate("view", "all digits trusted");
    if (r.trustedDigits == 1) return QCoreApplication::translate("view", "1 trusted digit");
    return QCoreApplication::translate("view", "%1 trusted digits").arg(r.trustedDigits);
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
    if (note == "software, no subnormals") return QCoreApplication::translate("view", "software, no subnormals");
    return fromStd(note);
}

QString translatedLabel(const std::string& label) {
    if (label == "Single") return QCoreApplication::translate("view", "Single");
    if (label == "Double") return QCoreApplication::translate("view", "Double");
    if (label == "Extended") return QCoreApplication::translate("view", "Extended");
    if (label == "Exact") return QCoreApplication::translate("view", "Exact");
    if (label == "Quadruple") return QCoreApplication::translate("view", "Quadruple");
    if (label == "Octuple") return QCoreApplication::translate("view", "Octuple");
    return fromStd(label);
}

}  // namespace

QString typeLabel(const TypeInfo& t) {
    const QString dot = QStringLiteral(" · ");
    QString s = translatedLabel(t.label) + dot + fromStd(t.cppName);
    if (t.type == NumberType::Exact) return s + dot + QCoreApplication::translate("view", "no rounding");
    s += dot + QCoreApplication::translate("view", "%1-bit").arg(t.storageBits) + dot + QCoreApplication::translate("view", "~%1 digits").arg(t.decimalDigits);
    if (!t.note.empty()) s += dot + translatedNote(t.note);
    return s;
}

QString valueHtml(const Result& r, const QString& noiseColor) {
    if (r.error) return {};
    if (r.exact) return fractionText(*r.exact).toHtmlEscaped();
    const Parts p = split(r.value, r.trustedDigits);
    if (p.noise.isEmpty()) return p.trusted + p.suffix;
    return p.trusted + QStringLiteral("<span style=\"color:%1\">|%2</span>").arg(noiseColor, p.noise) + p.suffix;
}

QString errorLine(const Result& r) {
    if (r.error) return {};
    if (r.exact) return QCoreApplication::translate("view", "exact · no rounding error");
    QString s = QStringLiteral("± ") + fromStd(r.bound) + QStringLiteral(" · ") + trustedText(r);
    if (!r.boundComplete) s += QStringLiteral(" · ") + QCoreApplication::translate("view", "incomplete: an uncertain argument was accepted");
    return s;
}

QString whyLine(const Result& r, const TypeInfo& t) {
    if (r.error || r.exact) return {};
    return QCoreApplication::translate("view", "input %1 · rounding %2 · library %3 · rounded operations: %4 · %5, %6-bit significand")
        .arg(fromStd(r.inputError), fromStd(r.roundingError), fromStd(r.libraryError))
        .arg(r.roundingOperations)
        .arg(fromStd(t.cppName))
        .arg(t.precisionBits);
}

QString verdict(const QString& conditionNumber) {
    const int e = exponentOf(conditionNumber);
    if (e <= 0) return QCoreApplication::translate("view", "well-conditioned");
    if (e <= 7) return QCoreApplication::translate("view", "moderately conditioned");
    return QCoreApplication::translate("view", "ill-conditioned: no algorithm can do better in this type");
}

QList<QPair<QString, QString>> details(const Result& r) {
    if (r.error) return {};
    QString measured = r.measuredAvailable ? fromStd(r.measured) : QCoreApplication::translate("view", "unavailable");
    if (r.measuredAvailable && !r.measurementReliable) measured += QStringLiteral(" (") + QCoreApplication::translate("view", "unreliable") + QStringLiteral(")");
    return {
        {QCoreApplication::translate("view", "Guaranteed bound"), fromStd(r.bound)},
        {QCoreApplication::translate("view", "Measured error"), measured},
        {QCoreApplication::translate("view", "Input error"), fromStd(r.inputError)},
        {QCoreApplication::translate("view", "Rounding error"), fromStd(r.roundingError)},
        {QCoreApplication::translate("view", "Library error"), fromStd(r.libraryError)},
        {QCoreApplication::translate("view", "Condition number κ"), fromStd(r.conditionNumber) + QStringLiteral(" · ") + verdict(fromStd(r.conditionNumber))},
        {QCoreApplication::translate("view", "Trusted digits"), QCoreApplication::translate("view", "%1 by the bound, %2 by the measurement").arg(r.trustedDigits).arg(r.trustedDigitsMeasured)},
        {QCoreApplication::translate("view", "Evaluated"), fromStd(r.expression)},
    };
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
    case ErrorCode::Cancelled: return QCoreApplication::translate("view", "Cancelled");
    }
    return {};
}

QString statisticsExpression(const QString& function, const QString& values) {
    const QStringList list = values.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    if (list.isEmpty()) return {};
    return function + "(" + list.join(", ") + ")";
}

}  // namespace view
