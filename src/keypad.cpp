#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

namespace {

Key fn(const QString& label, const QString& name) { return {label, name + "(", name}; }
Key text(const QString& label, const QString& insert, const QString& function = {}) { return {label, insert, function}; }
Key action(const QString& label, KeyAction a, int span = 1) { return {label, {}, {}, a, span}; }

}  // namespace

const QList<Key>& keypad() {
    static const QList<Key> keys{
        fn("sin", "sin"), fn("cos", "cos"), fn("tan", "tan"), fn("ln", "ln"), fn("log", "log"),
        text("√", "√", "sqrt"), text("x²", "²"), text("xʸ", "^"),
        fn("asin", "asin"), fn("acos", "acos"), fn("atan", "atan"), text("eˣ", "exp(", "exp"), text("10ˣ", "10^"),
        text("∛", "∛", "cbrt"), text("x³", "³"), text("ⁿ√", "root(", "root"),
        fn("sinh", "sinh"), fn("cosh", "cosh"), fn("tanh", "tanh"), text("π", "π", "pi"), text("e", "e", "e"),
        text("n!", "!"), text("%", "%"), fn("abs", "abs"),
        fn("asinh", "asinh"), fn("acosh", "acosh"), fn("atanh", "atanh"), fn("nCr", "nCr"), fn("nPr", "nPr"),
        fn("mod", "mod"), fn("gcd", "gcd"), fn("lcm", "lcm"),
        text("7", "7"), text("8", "8"), text("9", "9"), text("÷", "÷"), text("(", "("), text(")", ")"),
        action("C", KeyAction::Clear), action("⌫", KeyAction::Backspace),
        text("4", "4"), text("5", "5"), text("6", "6"), text("×", "×"), action("M+", KeyAction::MemoryAdd),
        action("M−", KeyAction::MemorySubtract), text("MR", "M"), action("MC", KeyAction::MemoryClear),
        text("1", "1"), text("2", "2"), text("3", "3"), text("−", "−"), text("Ans", "Ans"), text(",", ", "),
        text("EXP", "e"), text("±", "-"),
        {"0", "0", {}, KeyAction::Insert, 2}, text(".", "."), text("+", "+"), action("=", KeyAction::Evaluate, 4),
    };
    return keys;
}

bool availableInExact(const Key& key) {
    if (key.function.isEmpty()) return true;
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        if (QString::fromStdString(f.name) == key.function) return f.exact;
    return true;
}
