#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QCoreApplication>

namespace {

Face put(const QString& label, const QString& insert, const QString& function = {}) {
    return {label, insert, function, KeyAction::Insert};
}
Face act(const QString& label, KeyAction action) { return {label, {}, {}, action}; }
Key digit(const QString& d) { return {d, put(d, d)}; }

}  // namespace

// Names that Spanish calculators spell differently are marked for translation; the engine accepts both.
const QList<QList<Key>>& keypad() {
    static const QList<QList<Key>> rows{
        {{"open", put("(", "(")}, {"close", put(")", ")")}, {"fraction", put("□/□", "/")},
         {"sqrt", put("√□", "√(", "sqrt")}},
        {{"square", put("x²", "²")}, {"power", put("x^□", "^")}, {"negative", put("(−)", "-")},
         {"reciprocal", put("x⁻¹", "^-1")}},
        {{"sin", put(QT_TRANSLATE_NOOP("keypad", "sin"), QT_TRANSLATE_NOOP("keypad", "sin("), "sin")},
         {"cos", put("cos", "cos(", "cos")},
         {"tan", put("tan", "tan(", "tan")},
         {"logBase", put("log□□", "log(", "log")},
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

const QList<KeyGroup>& directKeys() {
    static const QList<KeyGroup> groups{
        {QT_TRANSLATE_NOOP("keypad", "Trigonometry"),
         {{"asin", put(QT_TRANSLATE_NOOP("keypad", "asin"), QT_TRANSLATE_NOOP("keypad", "asin("), "asin")},
          {"acos", put(QT_TRANSLATE_NOOP("keypad", "acos"), QT_TRANSLATE_NOOP("keypad", "acos("), "acos")},
          {"atan", put(QT_TRANSLATE_NOOP("keypad", "atan"), QT_TRANSLATE_NOOP("keypad", "atan("), "atan")}}},
        {QT_TRANSLATE_NOOP("keypad", "Hyperbolic"),
         {{"sinh", put(QT_TRANSLATE_NOOP("keypad", "sinh"), QT_TRANSLATE_NOOP("keypad", "sinh("), "sinh")},
          {"cosh", put("cosh", "cosh(", "cosh")},
          {"tanh", put("tanh", "tanh(", "tanh")},
          {"asinh", put(QT_TRANSLATE_NOOP("keypad", "asinh"), QT_TRANSLATE_NOOP("keypad", "asinh("), "asinh")},
          {"acosh", put(QT_TRANSLATE_NOOP("keypad", "acosh"), QT_TRANSLATE_NOOP("keypad", "acosh("), "acosh")},
          {"atanh", put(QT_TRANSLATE_NOOP("keypad", "atanh"), QT_TRANSLATE_NOOP("keypad", "atanh("), "atanh")}}},
        {QT_TRANSLATE_NOOP("keypad", "Powers and roots"),
         {{"cube", put("x³", "³")}, {"cbrt", put("∛", "∛(", "cbrt")}, {"root", put("ⁿ√", "root(", "root")},
          {"power10", put("10ˣ", "10^")}, {"exp", put("eˣ", "exp(", "exp")}, {"log", put("log", "log(", "log")}}},
        {QT_TRANSLATE_NOOP("keypad", "Numbers"),
         {{"factorial", put("x!", "!")}, {"abs", put("Abs", "abs(", "abs")}, {"percent", put("%", "%")},
          {"mod", put("mod", "mod(", "mod")}, {"npr", put("nPr", "nPr(", "nPr")}, {"ncr", put("nCr", "nCr(", "nCr")},
          {"gcd", put(QT_TRANSLATE_NOOP("keypad", "gcd"), QT_TRANSLATE_NOOP("keypad", "gcd("), "gcd")},
          {"lcm", put(QT_TRANSLATE_NOOP("keypad", "lcm"), QT_TRANSLATE_NOOP("keypad", "lcm("), "lcm")},
          {"comma", put(",", ", ")}}},
        {QT_TRANSLATE_NOOP("keypad", "Constants and memory"),
         {{"pi", put("π", "π", "pi")}, {"e", put("e", "e", "e")}, {"memorySubtract", act("M−", KeyAction::MemorySubtract)},
          {"memory", put("M", "M")}, {"memoryClear", act("MC", KeyAction::MemoryClear)}}},
    };
    return groups;
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
