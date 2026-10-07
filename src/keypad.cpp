#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QCoreApplication>

namespace {

Face put(const QString& label, const QString& insert, const QString& function = {}) {
    return {label, insert, function, KeyAction::Insert};
}
Face act(const QString& label, KeyAction action) { return {label, {}, {}, action}; }
// A key that opens a template; `fill` is typed into its box at once, and the cursor leaves it (x², x⁻¹).
Face shape(const QString& label, Template kind, const QString& function = {}, const QString& fill = {}) {
    return {label, fill, function, KeyAction::Template, kind};
}
Key digit(const QString& d) { return {d, put(d, d)}; }

}  // namespace

// Names that Spanish calculators spell differently are marked for translation; the engine accepts both.
const QList<QList<Key>>& keypad() {
    static const QList<QList<Key>> rows{
        {{"open", put("(", "(")}, {"close", put(")", ")")}, {"fraction", shape("□/□", Template::Fraction)},
         {"sqrt", shape("√□", Template::Sqrt, "sqrt")}},
        {{"square", shape("x²", Template::Power, {}, "2")}, {"power", shape("x^□", Template::Power)}, {"negative", put("(−)", "-")},
         {"reciprocal", shape("x⁻¹", Template::Power, {}, "−1")}},
        {{"sin", put(QT_TRANSLATE_NOOP("keypad", "sin"), QT_TRANSLATE_NOOP("keypad", "sin("), "sin")},
         {"cos", put("cos", "cos(", "cos")},
         {"tan", put("tan", "tan(", "tan")},
         {"logBase", shape("log□□", Template::LogBase, "log")},
         {"ln", put("ln", "ln(", "ln")},
         {"memoryAdd", act("M+", KeyAction::MemoryAdd)}},
        {digit("7"), digit("8"), digit("9"), {"delete", act("DEL", KeyAction::Backspace)}, {"clear", act("AC", KeyAction::Clear)}},
        {digit("4"), digit("5"), digit("6"), {"multiply", put("×", "×")}, {"divide", put("÷", "÷")}},
        {digit("1"), digit("2"), digit("3"), {"plus", put("+", "+")}, {"minus", put("−", "−")}},
        {digit("0"), {"point", put(".", ".")}, {"exponent", put("×10ˣ", "e")}, {"ans", put("Ans", "Ans")},
         {"equals", act("=", KeyAction::Evaluate)}},
    };
    return rows;
}

const QList<Key>& cursorPad() {
    static const QList<Key> keys{
        {"up", act("▲", KeyAction::Up)},
        {"left", act("◄", KeyAction::Left)},
        {"right", act("►", KeyAction::Right)},
        {"down", act("▼", KeyAction::Down)},
    };
    return keys;
}

const QList<Key>& memoryKeys() {
    static const QList<Key> keys{
        {"memoryStore", act("MS", KeyAction::MemoryStore)},
        {"memory", put("M", "M")},
        {"memorySubtract", act("M−", KeyAction::MemorySubtract)},
        {"memoryClear", act("MC", KeyAction::MemoryClear)},
    };
    return keys;
}

const QList<KeySection>& keySections() {
    static const QList<KeySection> sections{
        {"numbers", QT_TRANSLATE_NOOP("keypad", "Numbers"),
         {{"factorial", put("x!", "!")}, {"abs", shape("abs", Template::Abs, "abs")}, {"percent", put("%", "%")},
          {"mod", put("mod", "mod(", "mod")}, {"npr", put("nPr", "nPr(", "nPr")}, {"ncr", put("nCr", "nCr(", "nCr")},
          {"gcd", put(QT_TRANSLATE_NOOP("keypad", "gcd"), QT_TRANSLATE_NOOP("keypad", "gcd("), "gcd")},
          {"lcm", put(QT_TRANSLATE_NOOP("keypad", "lcm"), QT_TRANSLATE_NOOP("keypad", "lcm("), "lcm")},
          {"comma", put(",", ", ")}}},
        {"hyperbolic", QT_TRANSLATE_NOOP("keypad", "Hyperbolic"),
         {{"sinh", put(QT_TRANSLATE_NOOP("keypad", "sinh"), QT_TRANSLATE_NOOP("keypad", "sinh("), "sinh")},
          {"cosh", put("cosh", "cosh(", "cosh")},
          {"tanh", put("tanh", "tanh(", "tanh")},
          {"asinh", put(QT_TRANSLATE_NOOP("keypad", "asinh"), QT_TRANSLATE_NOOP("keypad", "asinh("), "asinh")},
          {"acosh", put(QT_TRANSLATE_NOOP("keypad", "acosh"), QT_TRANSLATE_NOOP("keypad", "acosh("), "acosh")},
          {"atanh", put(QT_TRANSLATE_NOOP("keypad", "atanh"), QT_TRANSLATE_NOOP("keypad", "atanh("), "atanh")}}},
        {"trigonometry", QT_TRANSLATE_NOOP("keypad", "Trigonometry"),
         {{"asin", put(QT_TRANSLATE_NOOP("keypad", "asin"), QT_TRANSLATE_NOOP("keypad", "asin("), "asin")},
          {"acos", put(QT_TRANSLATE_NOOP("keypad", "acos"), QT_TRANSLATE_NOOP("keypad", "acos("), "acos")},
          {"atan", put(QT_TRANSLATE_NOOP("keypad", "atan"), QT_TRANSLATE_NOOP("keypad", "atan("), "atan")}}},
        {"powers", QT_TRANSLATE_NOOP("keypad", "Powers, roots and logs"),
         {{"cube", shape("x³", Template::Power, {}, "3")}, {"cbrt", shape("∛", Template::Cbrt, "cbrt")}, {"root", shape("ⁿ√", Template::Root, "root")},
          {"power10", shape("10ˣ", Template::Pow10)}, {"exp", shape("eˣ", Template::Exp, "exp")}, {"log", put("log", "log(", "log")}}},
        {"constants", QT_TRANSLATE_NOOP("keypad", "Constants"), {{"pi", put("π", "π", "pi")}, {"e", put("e", "e", "e")}}},
        {"statistics", QT_TRANSLATE_NOOP("keypad", "Statistics"), statisticsKeys()},
    };
    return sections;
}

const QList<Key>& statisticsKeys() {
    static const QList<Key> keys{
        {"mean", put("mean", "mean(", "mean")},   {"median", put("median", "median(", "median")},
        {"var", put("var", "var(", "var")},       {"stdev", put("stdev", "stdev(", "stdev")},
        {"varp", put("varp", "varp(", "varp")},   {"stdevp", put("stdevp", "stdevp(", "stdevp")},
    };
    return keys;
}

const QStringList& defaultCommon() {
    static const QStringList ids{"asin", "acos", "atan", "pi", "e", "factorial", "power10", "exp", "cube", "cbrt", "root", "abs"};
    return ids;
}

QList<Key> everyDirectKey() {
    QList<Key> all = memoryKeys();
    for (const KeySection& section : keySections()) all += section.keys;
    return all;
}

Key directKey(const QString& id) {
    for (const Key& key : everyDirectKey())
        if (key.id == id) return key;
    return Key{};
}

bool available(const Face& face, bool exact) {
    if (!exact || face.function.isEmpty()) return true;
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        if (QString::fromStdString(f.name) == face.function) return f.exact;
    return true;
}

QString translated(const QString& text) {
    return QCoreApplication::translate("keypad", text.toUtf8().constData());
}
