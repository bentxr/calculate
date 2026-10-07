#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace {

Face put(const QString& label, const QString& insert, const QString& function = {}) {
    return {label, insert, function, KeyAction::Insert};
}
Face act(const QString& label, KeyAction action) { return {label, {}, {}, action}; }
// A key whose text is typed as the keyboard types it (so letters before a ( become a function's piece).
Face type(const QString& label, const QString& text) { return {label, text, {}, KeyAction::Type}; }
// A key that opens a template; `fill` is typed into its box at once, and the cursor leaves it (x², x⁻¹).
Face shape(const QString& label, Template kind, const QString& function = {}, const QString& fill = {}) {
    return {label, fill, function, KeyAction::Template, kind};
}
Key digit(const QString& d) { return {d, put(d, d)}; }

}  // namespace

// Names that Spanish calculators spell differently are marked for translation; the engine accepts both.
const QList<QList<Key>>& keypad() {
    static const QList<QList<Key>> rows{
        {{"open", type("(", "(")}, {"close", put(")", ")")}, {"fraction", shape("□/□", Template::Fraction)},
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
        {"undo", act("↶", KeyAction::Undo)},
        {"redo", act("↷", KeyAction::Redo)},
    };
    return keys;
}

namespace {

// a to z, then ⇧, _ and the space: names, comments and anything else typed without a keyboard.
QList<Key> letterKeys() {
    QList<Key> keys;
    for (char c = 'a'; c <= 'z'; ++c) {
        const QString letter(QChar::fromLatin1(c));
        keys.append({"letter" + letter.toUpper(), type(letter, letter)});
    }
    keys.append({"shift", act("⇧", KeyAction::Shift)});
    keys.append({"underscore", type("_", "_")});
    keys.append({"space", type("␣", " ")});
    return keys;
}

}  // namespace

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
        {"letters", QT_TRANSLATE_NOOP("keypad", "Letters"), letterKeys()},
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

bool canBeCommon(const QStringList& ids) {
    if (ids.size() > commonLimit || QSet<QString>(ids.begin(), ids.end()).size() != ids.size()) return false;
    QSet<QString> sectionKeys;
    for (const KeySection& section : keySections())
        for (const Key& key : section.keys) sectionKeys.insert(key.id);
    return std::all_of(ids.begin(), ids.end(), [&](const QString& id) { return sectionKeys.contains(id); });
}

QList<SearchEntry> searchEntries() {
    QList<SearchEntry> entries;
    for (const KeySection& section : keySections())
        if (section.id != QLatin1String("letters"))  // letters are typing, not something to find
            for (const Key& key : section.keys) entries.append({section.title, key.face, {}});
    for (const QList<Key>& row : keypad())
        for (const Key& key : row)
            if (!key.face.function.isEmpty()) entries.append({QT_TRANSLATE_NOOP("keypad", "Main keys"), key.face, {}});
    return entries + extraSearchEntries();
}

const QList<SearchEntry>& extraSearchEntries() {
    static const QList<SearchEntry> entries;
    return entries;
}

bool searchMatches(const SearchEntry& entry, const QString& text) {
    const QString wanted = text.trimmed().toLower();
    if (wanted.isEmpty()) return true;
    const Face& f = entry.face;
    for (const QString& s : {f.label, translated(f.label), f.insert, translated(f.insert), f.function, entry.title,
                             entry.group, translated(entry.group)})
        if (s.toLower().contains(wanted)) return true;
    return false;
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

const QList<QPair<QString, QStringList>>& alternates() {
    static const QList<QPair<QString, QStringList>> list{
        {"sin", {"asin", "sinh", "asinh"}},
        {"cos", {"acos", "cosh", "acosh"}},
        {"tan", {"atan", "tanh", "atanh"}},
        {"ln", {"log", "exp"}},
        {"logBase", {"log"}},
        {"square", {"cube"}},
        {"sqrt", {"cbrt", "root"}},
        {"memoryAdd", {"memorySubtract", "memoryStore", "memory", "memoryClear"}},
    };
    return list;
}

QString spokenName(const Key& key) {
    static const QHash<QString, const char*> words{
        {"□/□", QT_TRANSLATE_NOOP("spoken", "fraction")},
        {"√□", QT_TRANSLATE_NOOP("spoken", "square root")},
        {"x²", QT_TRANSLATE_NOOP("spoken", "square")},
        {"x^□", QT_TRANSLATE_NOOP("spoken", "power")},
        {"(−)", QT_TRANSLATE_NOOP("spoken", "negative")},
        {"x⁻¹", QT_TRANSLATE_NOOP("spoken", "reciprocal")},
        {"log□□", QT_TRANSLATE_NOOP("spoken", "logarithm in a base")},
        {"×10ˣ", QT_TRANSLATE_NOOP("spoken", "times ten to the power")},
        {"▲", QT_TRANSLATE_NOOP("spoken", "up")},
        {"◄", QT_TRANSLATE_NOOP("spoken", "left")},
        {"►", QT_TRANSLATE_NOOP("spoken", "right")},
        {"▼", QT_TRANSLATE_NOOP("spoken", "down")},
        {"DEL", QT_TRANSLATE_NOOP("spoken", "delete")},
        {"AC", QT_TRANSLATE_NOOP("spoken", "clear all")},
        {"=", QT_TRANSLATE_NOOP("spoken", "equals")},
        {"+", QT_TRANSLATE_NOOP("spoken", "plus")},
        {"−", QT_TRANSLATE_NOOP("spoken", "minus")},
        {"×", QT_TRANSLATE_NOOP("spoken", "times")},
        {"÷", QT_TRANSLATE_NOOP("spoken", "divided by")},
        {".", QT_TRANSLATE_NOOP("spoken", "point")},
        {"(", QT_TRANSLATE_NOOP("spoken", "open parenthesis")},
        {")", QT_TRANSLATE_NOOP("spoken", "close parenthesis")},
        {"x³", QT_TRANSLATE_NOOP("spoken", "cube")},
        {"∛", QT_TRANSLATE_NOOP("spoken", "cube root")},
        {"ⁿ√", QT_TRANSLATE_NOOP("spoken", "root")},
        {"10ˣ", QT_TRANSLATE_NOOP("spoken", "ten to the power")},
        {"eˣ", QT_TRANSLATE_NOOP("spoken", "e to the power")},
        {"x!", QT_TRANSLATE_NOOP("spoken", "factorial")},
        {"%", QT_TRANSLATE_NOOP("spoken", "percent")},
        {",", QT_TRANSLATE_NOOP("spoken", "separator")},
        {"π", QT_TRANSLATE_NOOP("spoken", "pi")},
        {"M+", QT_TRANSLATE_NOOP("spoken", "memory plus")},
        {"M−", QT_TRANSLATE_NOOP("spoken", "memory minus")},
        {"MS", QT_TRANSLATE_NOOP("spoken", "memory store")},
        {"M", QT_TRANSLATE_NOOP("spoken", "memory")},
        {"MC", QT_TRANSLATE_NOOP("spoken", "memory clear")},
        {"↶", QT_TRANSLATE_NOOP("spoken", "undo")},
        {"↷", QT_TRANSLATE_NOOP("spoken", "redo")},
        {"⇧", QT_TRANSLATE_NOOP("spoken", "shift")},
        {"␣", QT_TRANSLATE_NOOP("spoken", "space")},
        {"_", QT_TRANSLATE_NOOP("spoken", "underscore")},
    };
    const auto word = words.constFind(key.face.label);
    return word == words.constEnd() ? translated(key.face.label) : QCoreApplication::translate("spoken", *word);
}
