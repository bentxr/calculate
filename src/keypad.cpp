#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QCoreApplication>

namespace {

Face put(const QString& label, const QString& insert, const QString& function = {}) {
    return {label, insert, function, KeyAction::Insert};
}
Face act(const QString& label, KeyAction action) { return {label, {}, {}, action}; }
Face none(const QString& label) { return {label, {}, {}, KeyAction::Unavailable}; }
Key digit(const QString& d, const QString& shift = {}) { return {d, put(d, d), none(shift), {}}; }

}  // namespace

// Legends from Casio's fx-570/991SP X II user guide. Names that Spanish calculators spell
// differently are marked for translation; the engine accepts both spellings.
const QList<QList<Key>>& keypad() {
    static const QList<QList<Key>> rows{
        {{"shift", act("SHIFT", KeyAction::Shift), {}, {}},
         {"alpha", act("ALPHA", KeyAction::Alpha), {}, {}},
         {"menu", act("MENU", KeyAction::Menu), act("CONFIG", KeyAction::Config), {}},
         {"on", act("ON", KeyAction::Clear), {}, {}}},
        {{"options", act("OPTN", KeyAction::Options), none("QR"), {}},
         {"calc", none("CALC"), none("SOLVE"), none("=")},
         {"integral", none("∫□"), none("d/dx"), none(":")},
         {"x", none("x"), none("Σ"), none("Π")}},
        {{"fraction", put("□/□", "/"), {}, none("∟")},
         {"sqrt", put("√□", "√(", "sqrt"), put("∛□", "∛(", "cbrt"), {}},
         {"square", put("x²", "²"), put("x³", "³"), {}},
         {"power", put("x^□", "^"), put("ⁿ√□", "root(", "root"), {}},
         {"logBase", put("log□□", "log(", "log"), put("10^□", "10^"), {}},
         {"ln", put("ln", "ln(", "ln"), put("e^□", "exp(", "exp"), {}}},
        {{"negative", put("(−)", "-"), put("log", "log(", "log"), none("A")},
         {"degrees", none("°'\""), none("FACT"), none("B")},
         {"reciprocal", put("x⁻¹", "^-1"), put("x!", "!"), none("C")},
         {"sin", put(QT_TRANSLATE_NOOP("keypad", "sin"), QT_TRANSLATE_NOOP("keypad", "sin("), "sin"),
          put(QT_TRANSLATE_NOOP("keypad", "asin"), QT_TRANSLATE_NOOP("keypad", "asin("), "asin"), none("D")},
         {"cos", put("cos", "cos(", "cos"),
          put(QT_TRANSLATE_NOOP("keypad", "acos"), QT_TRANSLATE_NOOP("keypad", "acos("), "acos"), none("E")},
         {"tan", put("tan", "tan(", "tan"),
          put(QT_TRANSLATE_NOOP("keypad", "atan"), QT_TRANSLATE_NOOP("keypad", "atan("), "atan"), none("F")}},
        {{"store", none("STO"), none("RECALL"), {}},
         {"engineering", none("ENG"), none("←"), {}},
         {"open", put("(", "("), put("Abs", "abs(", "abs"), none("Simp")},
         {"close", put(")", ")"), put(",", ", "), none("x")},
         {"toggle", none("S⇔D"), {}, {}},
         {"memoryAdd", act("M+", KeyAction::MemoryAdd), act("M−", KeyAction::MemorySubtract), put("M", "M")}},
        {digit("7", "CONST"), digit("8", "CONV"), digit("9", "RESET"),
         {"delete", act("DEL", KeyAction::Backspace), none("INS"), {}},
         {"clear", act("AC", KeyAction::Clear), none("OFF"), {}}},
        {digit("4"), digit("5"), digit("6"),
         {"multiply", put("×", "×"), put("nPr", "nPr(", "nPr"),
          put(QT_TRANSLATE_NOOP("keypad", "gcd"), QT_TRANSLATE_NOOP("keypad", "gcd("), "gcd")},
         {"divide", put("÷", "÷"), put("nCr", "nCr(", "nCr"),
          put(QT_TRANSLATE_NOOP("keypad", "lcm"), QT_TRANSLATE_NOOP("keypad", "lcm("), "lcm")}},
        {digit("1"), digit("2"), digit("3"),
         {"plus", put("+", "+"), none("Pol"), none("Int")},
         {"minus", put("−", "−"), none("Rec"), none("Intg")}},
        {digit("0", "Rnd"),
         {"point", put(".", "."), none("Ran#"), none("RanInt")},
         {"exponent", put("×10ˣ", "e"), put("π", "π", "pi"), put("e", "e", "e")},
         {"ans", put("Ans", "Ans"), put("%", "%"), none("PreAns")},
         {"equals", act("=", KeyAction::Evaluate), {}, {}}},
    };
    return rows;
}

const QList<Key>& cursorPad() {
    static const QList<Key> keys{
        {"up", act("▲", KeyAction::Up), {}, {}},
        {"left", act("◄", KeyAction::Left), {}, {}},
        {"right", act("►", KeyAction::Right), {}, {}},
        {"down", act("▼", KeyAction::Down), {}, {}},
    };
    return keys;
}

const QList<Face>& optionsMenu() {
    static const QList<Face> faces{
        put(QT_TRANSLATE_NOOP("keypad", "sinh"), QT_TRANSLATE_NOOP("keypad", "sinh("), "sinh"),
        put("cosh", "cosh(", "cosh"),
        put("tanh", "tanh(", "tanh"),
        put(QT_TRANSLATE_NOOP("keypad", "asinh"), QT_TRANSLATE_NOOP("keypad", "asinh("), "asinh"),
        put(QT_TRANSLATE_NOOP("keypad", "acosh"), QT_TRANSLATE_NOOP("keypad", "acosh("), "acosh"),
        put(QT_TRANSLATE_NOOP("keypad", "atanh"), QT_TRANSLATE_NOOP("keypad", "atanh("), "atanh"),
        put("mod", "mod(", "mod"),
        act("MC", KeyAction::MemoryClear),
    };
    return faces;
}

bool available(const Face& face, bool exact) {
    if (face.action == KeyAction::Unavailable) return false;
    if (!exact || face.function.isEmpty()) return true;
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        if (QString::fromStdString(f.name) == face.function) return f.exact;
    return true;
}

QString translated(const QString& text) {
    return QCoreApplication::translate("keypad", text.toUtf8().constData());
}
