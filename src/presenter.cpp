#include "presenter.hpp"

#include "settings.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

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

// U, the bound plus the leading uncertainty, to its two digits: shown after the value as "± U".
void addUncertainty(const Result& r, QString& uncertainty, QString& exponent) {
    if (r.uncertainInputs.empty()) return;
    const ValueParts u = split(r.uncertaintyShown, 2);
    uncertainty = u.trusted;
    exponent = u.exponent;
}

ValueParts valueParts(const Result& r) {
    if (r.error || r.exact || r.commentOnly) return {};
    if (r.binaryValue) {  // too long for decimals: exactly, as an odd whole number times a power of two
        const BinaryValue& b = *r.binaryValue;
        ValueParts p;
        const QString power = QStringLiteral("2^") + (b.exponent2 < 0 ? minus() + QString::number(-b.exponent2) : QString::number(b.exponent2));
        p.trusted = (b.negative ? minus() : QString()) + (b.significand == "1" ? power : fromStd(b.significand) + QStringLiteral(" × ") + power);
        p.unit = fromStd(r.unit);
        return p;
    }
    ValueParts p = split(r.value, r.trustedDigitsWithUncertainty);  // the bar where the uncertainty starts
    addUncertainty(r, p.uncertainty, p.uncertaintyExponent);
    p.unit = fromStd(r.unit);
    return p;
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
    addUncertainty(r, p.uncertainty, p.uncertaintyExponent);
    p.unit = fromStd(r.unit);
    return p;
}

QString verdict(const QString& conditionNumber) {
    const int e = exponentOf(conditionNumber);
    if (e <= 0) return QCoreApplication::translate("view", "well-conditioned");
    if (e <= 7) return QCoreApplication::translate("view", "moderately conditioned");
    return QCoreApplication::translate("view", "ill-conditioned: no algorithm can do better in this type");
}

QString offBy(const Result& r) {
    if (!r.conversion || r.conversion->note.empty()) return {};
    const std::string& note = r.conversion->note;
    return number(note.substr(note.find_last_of(' ') + 1));
}

QString conversionText(const Result& r) {
    if (!r.conversion) return {};
    const QString text = fromStd(r.conversion->text);
    return settings::decimalComma() ? withDecimalComma(text) : text;
}

std::optional<ValueParts> conversionParts(const Result& r) {
    if (!r.conversion || !r.conversion->parts) return std::nullopt;
    const NumberParts& n = *r.conversion->parts;
    ValueParts p;
    p.trusted = (n.negative ? minus() : QString()) + fromStd(n.trusted);
    p.noise = fromStd(n.noise);
    if (n.hasExponent) p.exponent = n.exponent10 < 0 ? minus() + QString::number(-n.exponent10) : QString::number(n.exponent10);
    (p.noise.isEmpty() ? p.trusted : p.noise) += fromStd(n.suffix);
    return p;
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
    if (r.conversion) {
        converted = {{"value", QCoreApplication::translate("view", "Value"), valueText(r)},
                     {"conversion", QCoreApplication::translate("view", "Shown as"), fromStd(r.conversion->target)}};
        if (!r.conversion->note.empty())  // "off by 3.3e-2": the number alone
            converted.append({"conversionNote", QCoreApplication::translate("view", "Off by"), offBy(r)});
    }
    const QString expression = settings::decimalComma() ? withDecimalComma(fromStd(r.expression)) : fromStd(r.expression);
    const DetailRow evaluated{"evaluated", QCoreApplication::translate("view", "Evaluated"), expression};
    const QString read = settings::decimalComma() ? withDecimalComma(fromStd(r.reading)) : fromStd(r.reading);
    const QList<DetailRow> reading{{"reading", QCoreApplication::translate("view", "Read as"), read}};
    // The user's uncertain inputs: both combinations (the leading one first), where they come from, and whether
    // first order can be trusted with them.
    QList<DetailRow> uncertainty;
    if (!r.uncertainInputs.empty()) {
        const QString worst = QCoreApplication::translate("view", "± %1 worst case").arg(number(r.uncertaintyLinear));
        const QString statistical = QCoreApplication::translate("view", "± %1 statistical").arg(number(r.uncertaintyQuadrature));
        const bool linear = r.uncertaintyRule == UncertaintyRule::Linear;
        uncertainty.append({"uncertainty", QCoreApplication::translate("view", "Uncertainty"),
                            (linear ? worst : statistical) + QStringLiteral(" · ") + (linear ? statistical : worst)});
        QStringList sources;
        for (const UncertainInput& u : r.uncertainInputs) {
            const QString name = settings::decimalComma() ? withDecimalComma(fromStd(u.name)) : fromStd(u.name);
            sources << name + QStringLiteral(": ") + number(u.contribution);
        }
        uncertainty.append({"sources", QCoreApplication::translate("view", "Uncertain inputs"), sources.join(QStringLiteral(" · "))});
        if (r.firstOrderChecked && !r.firstOrderReliable)
            uncertainty.append({"firstorder", QCoreApplication::translate("view", "First order"),
                                r.firstOrderObserved == "inf"
                                    ? QCoreApplication::translate("view", "unreliable: an input shifted by its limit leaves a function's domain")
                                    : QCoreApplication::translate("view", "unreliable: at the corners the result moved by %1").arg(number(r.firstOrderObserved))});
    }
    QList<DetailRow> unit;
    if (!r.unit.empty()) unit.append({"unit", QCoreApplication::translate("view", "Unit"), fromStd(r.unit)});
    if (r.exact)
        return converted
               + QList<DetailRow>{{"exact", QCoreApplication::translate("view", "Error"), QCoreApplication::translate("view", "exact · no rounding error")}}
               + uncertainty
               + QList<DetailRow>{{"type", QCoreApplication::translate("view", "Number type"),
                                   QCoreApplication::translate("view", "%1, exact fractions").arg(fromStd(t.cppName))},
                                  evaluated}
               + unit + reading;
    QString measured = r.measuredAvailable ? number(r.measured) : QCoreApplication::translate("view", "unavailable");
    if (r.measuredAvailable && !r.measurementReliable) measured += QStringLiteral(" (") + QCoreApplication::translate("view", "unreliable") + QStringLiteral(")");
    QList<DetailRow> rows{
        {"bound", QCoreApplication::translate("view", "Guaranteed bound"), number(r.bound)},
        {"measured", QCoreApplication::translate("view", "Measured error"), measured},
        {"trusted", QCoreApplication::translate("view", "Trusted digits"),
         uncertainty.isEmpty()
             ? QCoreApplication::translate("view", "%1 by the bound, %2 by the measurement").arg(r.trustedDigits).arg(r.trustedDigitsMeasured)
             : QCoreApplication::translate("view", "%1 by the bound, %2 by the measurement, %3 with the uncertainty")
                   .arg(r.trustedDigits).arg(r.trustedDigitsMeasured).arg(r.trustedDigitsWithUncertainty)},
        {"condition", QCoreApplication::translate("view", "Condition number κ"), number(r.conditionNumber) + QStringLiteral(" · ") + verdict(fromStd(r.conditionNumber))},
        {"input", QCoreApplication::translate("view", "Input error"), number(r.inputError)},
        {"rounding", QCoreApplication::translate("view", "Rounding error"), number(r.roundingError)},
        {"library", QCoreApplication::translate("view", "Library error"), number(r.libraryError)},
        {"operations", QCoreApplication::translate("view", "Rounded operations"), QString::number(r.roundingOperations)},
        {"type", QCoreApplication::translate("view", "Number type"),
         QCoreApplication::translate("view", "%1, %2-bit significand").arg(fromStd(t.cppName)).arg(t.precisionBits)},
    };
    for (int i = 0; i < uncertainty.size(); ++i) rows.insert(5 + i, uncertainty[i]);  // after the input error
    if (!r.boundComplete)
        rows.append({"incomplete", QCoreApplication::translate("view", "Incomplete"),
                     QCoreApplication::translate("view", "an uncertain argument was accepted")});
    if (r.binaryValue)
        rows.append({"binary", QCoreApplication::translate("view", "Written in binary"),
                     QCoreApplication::translate("view", "the decimal would need more than 20 000 digits")});
    rows.append(evaluated);
    rows += unit;
    rows += reading;
    return converted + rows;
}

QString explanation(const QString& key) {
    if (key == "field:stored")
        return QCoreApplication::translate("view", "The value the format actually stores for this number, written out exactly.");
    if (key.startsWith(QStringLiteral("field:"))) return explanation(key.mid(6));  // the stored rows' texts
    if (key == "value")
        return QCoreApplication::translate("view", "The result as computed in this number type; the screen shows it converted.");
    if (key == "conversionNote")
        return QCoreApplication::translate("view", "The shown fraction differs from the computed value by this much.");
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
                                                   "0.1, for example, has no exact binary form (floatError shows it for one number).");
    if (key == "binary")
        return QCoreApplication::translate("view", "This value is written exactly as an odd whole number times a power of two, because "
                                                   "its decimal expansion is too long to show.");
    if (key == "bits")
        return QCoreApplication::translate("view", "The bits the computer stores for this result: sign, exponent and fraction, in the result's own type.");
    if (key == "hex") return QCoreApplication::translate("view", "The same bits written in hexadecimal, four bits per digit.");
    if (key == "class")
        return QCoreApplication::translate("view", "What kind of value this is (zero, subnormal, normal…), its power of two, and the "
                                                   "exponent field that encodes it (the power plus the bias).");
    if (key == "ulp")
        return QCoreApplication::translate("view", "Unit in the last place: the gap between neighbouring values of this type at this size. "
                                                   "Rounding moves a value by at most half of it.");
    if (key == "below")
        return QCoreApplication::translate("view", "The nearest value this type can store below this one: nothing in between exists in this type.");
    if (key == "above")
        return QCoreApplication::translate("view", "The nearest value this type can store above this one: nothing in between exists in this type.");
    if (key == "unit")
        return QCoreApplication::translate("view", "The SI unit of the result, followed from the units of the constants in it. "
                                                   "Plain numbers have none.");
    if (key == "uncertainty")
        return QCoreApplication::translate("view", "How far the result can move because of the uncertainty of your inputs (values written "
                                                   "with ±, whose ± is a limit, and measured constants, given three standard uncertainties). "
                                                   "The worst case adds every contribution: it is a limit, to first order, whatever the inputs' "
                                                   "correlations. The statistical figure combines them in quadrature: an estimate of the usual "
                                                   "spread if the inputs are independent, not a limit.");
    if (key == "sources")
        return QCoreApplication::translate("view", "Each uncertain input and how much it moves the result: its uncertainty times how "
                                                   "sensitive the result is to it. The largest comes first.");
    if (key == "firstorder")
        return QCoreApplication::translate("view", "The uncertainty is propagated with derivatives, which is accurate when it is small. "
                                                   "The inputs were also shifted to the ends of their limits: the result moved by more than "
                                                   "the estimate says, so the limit above is not safe.");
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
    if (key == "reading")
        return QCoreApplication::translate("view", "How the calculator read the expression: every operation in parentheses, in the order it is done.");
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
    if (form == CopyForm::Concise) return decimal(fromStd(r.concise));  // empty without an error or an uncertainty
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
    case WarningCode::FirstOrderUnreliable:
        return QCoreApplication::translate("view", "The uncertainty may be larger than shown: first order is unreliable here");
    case WarningCode::UnitsDiffer: {  // the engine's note names the operands: "the units of c (m·s⁻¹) and 1 (none) differ"
        const QString message = fromStd(w.message);
        static const QRegularExpression differ(QStringLiteral("^the units of (.*) \\([^()]*\\) and (.*) \\([^()]*\\) differ$"));
        static const QRegularExpression needs(QStringLiteral("^(.*) needs a number without a unit; "));
        if (const QRegularExpressionMatch m = differ.match(message); m.hasMatch())
            return QCoreApplication::translate("view", "The units of %1 and %2 differ").arg(m.captured(1), m.captured(2));
        if (const QRegularExpressionMatch m = needs.match(message); m.hasMatch())
            return QCoreApplication::translate("view", "%1 needs a number without a unit").arg(m.captured(1));
        return message;
    }
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
    case ErrorCode::ArgumentTooLarge: return QCoreApplication::translate("view", "The arguments of %1 are too large to compute accurately").arg(name);
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
    if (!parts.uncertainty.isEmpty()) s += " ± " + parts.uncertainty;
    if (!parts.uncertaintyExponent.isEmpty()) s += "×10^" + parts.uncertaintyExponent;
    if (!parts.unit.isEmpty()) s += " " + parts.unit;
    return s;
}

QString oneLine(const FractionParts& parts) {
    if (parts.denominator == "1") return parts.sign + parts.numerator;
    QString s = parts.sign + parts.numerator + "/" + parts.denominator;
    if (!parts.decimal.isEmpty()) s += " = " + parts.sign + parts.decimal;
    if (!parts.recurring.isEmpty()) s += "(" + parts.recurring + ")";
    if (!parts.uncertainty.isEmpty()) s += " ± " + parts.uncertainty;
    if (!parts.uncertaintyExponent.isEmpty()) s += "×10^" + parts.uncertaintyExponent;
    if (!parts.unit.isEmpty()) s += " " + parts.unit;
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

HintParts argumentHint(const QString& name, int argument) {
    for (const calculate_core::FunctionDescription& f : calculate_core::functions()) {
        bool named = QString::fromStdString(f.name) == name;
        for (const std::string& alias : f.aliases) named = named || QString::fromStdString(alias) == name;
        if (!named || f.arguments.empty()) continue;
        const QString separator = settings::decimalComma() ? QStringLiteral("; ") : QStringLiteral(", ");
        const bool repeated = f.maxArgs < 0;
        const int shown = repeated ? std::max(f.minArgs, argument + 1) : static_cast<int>(f.arguments.size());
        QStringList parts;  // each argument with what comes before it
        for (int k = 0; k < shown; ++k) {
            const QString argumentName = QString::fromStdString(f.arguments[repeated ? 0 : static_cast<std::size_t>(k)].name);
            const QString open = !repeated && k >= f.minArgs ? QStringLiteral("[") : QString();
            parts << open + (k > 0 ? separator : QString()) + argumentName;
        }
        HintParts h;
        h.before = name + QLatin1Char('(');
        for (int k = 0; k < shown; ++k) {
            const QString& part = parts[k];
            const QString argumentName = QString::fromStdString(f.arguments[repeated ? 0 : static_cast<std::size_t>(k)].name);
            if (k < argument) h.before += part;
            else if (k == argument) {
                h.before += part.chopped(argumentName.size());
                h.current = argumentName;
            } else h.after += part;
        }
        if (repeated) h.after += separator + QStringLiteral("…");
        for (int k = f.minArgs; !repeated && k < shown; ++k) h.after += QLatin1Char(']');
        h.after += QLatin1Char(')');
        if (h.current.isEmpty()) return {};  // past the last argument
        return h;
    }
    return {};
}

namespace {

// Thin spaces every four characters from the left.
QString grouped(const QString& text) {
    QString s;
    for (int i = 0; i < text.size(); ++i) {
        if (i > 0 && i % 4 == 0) s += QChar(0x2009);
        s += text[i];
    }
    return s;
}

// A finite stored value written out: positional for −7 <= exponent < 21, else d.ddd…e−N; `ascii` uses - for minus.
QString writtenOut(const calculate_core::Digits& d, bool ascii) {
    const QString minusSign = ascii ? QStringLiteral("-") : minus();
    const QString digits = fromStd(d.digits);
    const long long e = d.exponent10;
    QString s = d.negative ? minusSign : QString();
    if (e >= -7 && e < 21) {
        if (e < 0) return s + QStringLiteral("0.") + QString(static_cast<int>(-e - 1), QChar('0')) + digits;
        QString whole = digits.left(static_cast<int>(e) + 1);
        whole += QString(qMax(0, static_cast<int>(e) + 1 - static_cast<int>(digits.size())), QChar('0'));
        const QString rest = digits.mid(static_cast<int>(e) + 1);
        return s + whole + (rest.isEmpty() ? QString() : QStringLiteral(".") + rest);
    }
    s += digits.left(1);
    if (digits.size() > 1) s += QStringLiteral(".") + digits.mid(1);
    return s + QStringLiteral("e") + (e < 0 ? minusSign + QString::number(-e) : QString::number(e));
}

// The value, or its power-of-two form when it is too long to write out.
QString number(const FloatBits& b, bool ascii) {
    const QString minusSign = ascii ? QStringLiteral("-") : minus();
    switch (b.valueClass) {
    case FloatClass::Infinite: return ascii ? (b.negative ? QStringLiteral("-inf") : QStringLiteral("inf")) : (b.negative ? minusSign : QString()) + QStringLiteral("∞");
    case FloatClass::QuietNaN:
    case FloatClass::SignalingNaN: return ascii ? QStringLiteral("nan") : QStringLiteral("NaN");
    default: break;
    }
    if (!b.value.digits.empty()) return writtenOut(b.value, ascii);
    const QString sign = b.negative ? minusSign : QString();
    const QString power = QStringLiteral("2^") + (b.exponent2 < 0 ? minusSign + QString::number(-b.exponent2) : QString::number(b.exponent2));
    if (b.significand.digits == "1" && b.significand.exponent10 == 0) return sign + power;
    return sign + writtenOut(b.significand, ascii) + (ascii ? QStringLiteral(" * ") : QStringLiteral(" × ")) + power;
}

}  // namespace

BitGroups bitGroups(const FloatBits& bits) {
    return {fromStd(bits.sign), grouped(fromStd(bits.exponent)), grouped(fromStd(bits.fraction))};
}

BitColours bitColours(bool dark) {
    if (dark) return {QStringLiteral("#ff8a80"), QStringLiteral("#82b1ff"), QStringLiteral("#8fe3a8")};
    return {QStringLiteral("#b03a2e"), QStringLiteral("#1f5fbf"), QStringLiteral("#1e7a46")};
}

QString bitsHtml(const BitGroups& g, const BitColours& c) {
    const auto span = [](const QString& colour, const QString& text) {
        return QStringLiteral("<span style=\"color:") + colour + QStringLiteral("\">") + text + QStringLiteral("</span>");
    };
    return span(c.sign, g.sign) + QStringLiteral(" ") + span(c.exponent, g.exponent) + QStringLiteral(" ") + span(c.fraction, g.fraction);
}

QString floatClassName(FloatClass c) {
    switch (c) {
    case FloatClass::Zero: return QCoreApplication::translate("view", "zero");
    case FloatClass::Subnormal: return QCoreApplication::translate("view", "subnormal");
    case FloatClass::Normal: return QCoreApplication::translate("view", "normal");
    case FloatClass::Infinite: return QCoreApplication::translate("view", "infinite");
    case FloatClass::QuietNaN: return QCoreApplication::translate("view", "quiet NaN");
    case FloatClass::SignalingNaN: return QCoreApplication::translate("view", "signaling NaN");
    case FloatClass::Noncanonical: return QCoreApplication::translate("view", "noncanonical");
    }
    return {};
}

QString exactNumber(const FloatBits& bits) { return number(bits, false); }

QString decimalText(const FloatBits& bits) { return number(bits, true); }

QString exactDecimal(const Digits& digits) { return writtenOut(digits, false); }

QList<DetailRow> conversionRows(const Result& r) {
    if (!r.conversion) return {};
    static const QList<QPair<QString, const char*>> labels{
        {"hex", QT_TRANSLATE_NOOP("view", "Hex")},          {"class", QT_TRANSLATE_NOOP("view", "Class")},
        {"stored", QT_TRANSLATE_NOOP("view", "Stored value")}, {"error", QT_TRANSLATE_NOOP("view", "Conversion error")},
        {"ulp", QT_TRANSLATE_NOOP("view", "ulp")},          {"below", QT_TRANSLATE_NOOP("view", "Next below")},
        {"above", QT_TRANSLATE_NOOP("view", "Next above")}, {"note", QT_TRANSLATE_NOOP("view", "Note")},
        {"base", QT_TRANSLATE_NOOP("view", "Base")},        {"trusted", QT_TRANSLATE_NOOP("view", "Trusted digits in this base")},
        {"width", QT_TRANSLATE_NOOP("view", "Width (bits)")}};
    QList<DetailRow> rows;
    for (const ConversionField& f : r.conversion->fields) {
        const QString label = fromStd(f.label);
        QString shown = label;  // unknown labels as they are
        for (const auto& [key, text] : labels)
            if (key == label) shown = QCoreApplication::translate("view", text);
        rows.append({QStringLiteral("field:") + label, shown, fromStd(f.value), false});
    }
    return rows;
}

QList<DetailRow> storedRows(const Result& r, const BitColours& colours) {
    if (!r.stored) return {};
    const FloatInspection& i = *r.stored;
    const FloatBits& b = i.stored;
    QString formatName;
    for (const FloatFormatInfo& f : floatFormats())
        if (f.format == i.format) formatName = fromStd(f.name);
    QList<DetailRow> rows;
    rows.append({"bits", QCoreApplication::translate("view", "Stored bits"), bitsHtml(bitGroups(b), colours), true});
    rows.append({"hex", QCoreApplication::translate("view", "Hex"), QStringLiteral("0x") + grouped(fromStd(b.hex)), false});
    const bool hasExponent = b.valueClass == FloatClass::Subnormal || b.valueClass == FloatClass::Normal;
    const QString exponent2 = b.exponent2 < 0 ? minus() + QString::number(-b.exponent2) : QString::number(b.exponent2);
    rows.append({"class", QCoreApplication::translate("view", "Class"),
                 hasExponent ? QCoreApplication::translate("view", "%1 · exponent %2 (field %3) · %4")
                                   .arg(floatClassName(b.valueClass), exponent2, QString::number(b.biasedExponent), formatName)
                             : floatClassName(b.valueClass) + QStringLiteral(" · ") + formatName,
                 false});
    if (i.hasNeighbours) {
        if (b.valueClass != FloatClass::Infinite) {
            const QString power = QStringLiteral("2^") + (i.ulpExponent < 0 ? minus() + QString::number(-i.ulpExponent) : QString::number(i.ulpExponent));
            rows.append({"ulp", QCoreApplication::translate("view", "ulp"),
                         i.ulp.digits.empty() ? power : power + QStringLiteral(" = ") + writtenOut(i.ulp, false), false});
        }
        rows.append({"below", QCoreApplication::translate("view", "Next below"), exactNumber(i.below), false});
        rows.append({"above", QCoreApplication::translate("view", "Next above"), exactNumber(i.above), false});
    }
    static const char* const notes[] = {QT_TRANSLATE_NOOP("view", "overflow"), QT_TRANSLATE_NOOP("view", "underflow"),
                                        QT_TRANSLATE_NOOP("view", "no subnormals")};  // the engine's notes, for lupdate
    (void)notes;
    if (!i.note.empty())
        rows.append({"note", QCoreApplication::translate("view", "Note"), QCoreApplication::translate("view", i.note.c_str()), false});
    return rows;
}

}  // namespace view
