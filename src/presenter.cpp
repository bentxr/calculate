#include "presenter.hpp"

#include <QCoreApplication>

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

QString valueHtml(const Result& r, DigitStyle style, const QString& noiseColor) {
    if (r.error) return {};
    if (r.exact) return fractionText(*r.exact).toHtmlEscaped();
    const Parts p = split(r.value, r.trustedDigits);
    if (p.noise.isEmpty()) return p.trusted + p.suffix;
    const QString noise = QStringLiteral("<span style=\"color:%1\">%2</span>").arg(noiseColor);
    switch (style) {
    case DigitStyle::Faded: return p.trusted + noise.arg(QStringLiteral("<i>%1</i>").arg(p.noise)) + p.suffix;
    case DigitStyle::Bold: return QStringLiteral("<b>%1</b>").arg(p.trusted) + noise.arg(p.noise) + p.suffix;
    case DigitStyle::Bar: return p.trusted + noise.arg(QStringLiteral("|") + p.noise) + p.suffix;
    }
    return {};
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

}  // namespace view
