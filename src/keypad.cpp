#include "keypad.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QCoreApplication>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace {

Face put(const QString& label, const QString& insert, const QString& function = {}) {
    return {label, insert, function, KeyAction::Insert, Template::Text, {}};
}
Face act(const QString& label, KeyAction action) { return {label, {}, {}, action, Template::Text, {}}; }
// A key whose text is typed as the keyboard types it (so letters before a ( become a function's piece).
Face type(const QString& label, const QString& text) { return {label, text, {}, KeyAction::Type, Template::Text, {}}; }
// A key that opens a template; `fill` is typed into its box at once, and the cursor leaves it (x², x⁻¹).
Face shape(const QString& label, Template kind, const QString& function = {}, const QString& fill = {}) {
    return {label, fill, function, KeyAction::Template, kind, {}};
}
Face tool(const QString& label, const QString& opens) { return {label, {}, {}, KeyAction::Tool, Template::Text, opens}; }
Key digit(const QString& d) { return {d, put(d, d)}; }

}  // namespace

// Names that Spanish calculators spell differently are marked for translation; the engine accepts both.
const QList<QList<Key>>& keypad() {
    static const QList<QList<Key>> rows{
        {{"open", type("(", "(")}, {"close", put(")", ")")}, {"fraction", shape("□/□", Template::Fraction)},
         {"sqrt", shape("√□", Template::Sqrt, "sqrt")}},
        {{"square", shape("x²", Template::Power, "sq", "2")}, {"power", shape("x^□", Template::Power)}, {"negative", put("(−)", "-")},
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
// The targets kept for the Programming section (none yet); every other target is in Show as.
QStringList programmingTargets() { return {}; }

// A one-tap key per target named, in the engine's order: "→fraction". A target that takes a number after it ("1/n")
// inserts its fixed part, leaving the number to the keys.
QList<Key> conversionKeys(const QStringList& names) {
    QList<Key> keys;
    for (const calculate_core::TargetDescription& target : calculate_core::conversionTargets()) {
        const QString name = QString::fromStdString(target.name);
        if (!names.contains(name)) continue;
        const QString insert = QStringLiteral("→") + (name.endsWith("/n") ? name.chopped(1) : name);
        keys.append({"to:" + name, put(QStringLiteral("→") + name, insert)});
    }
    return keys;
}

// Every target not kept for another section.
QStringList showAsTargets() {
    QStringList names;
    for (const calculate_core::TargetDescription& target : calculate_core::conversionTargets())
        if (!programmingTargets().contains(QString::fromStdString(target.name))) names << QString::fromStdString(target.name);
    return names;
}

QList<Key> letterKeys() {
    QList<Key> keys;
    for (char c = 'a'; c <= 'z'; ++c) {
        const QString letter(QChar::fromLatin1(c));
        keys.append({"letter" + letter.toUpper(), type(letter, letter)});
    }
    keys.append({"shift", act("⇧", KeyAction::Shift)});
    keys.append({"underscore", type("_", "_")});
    keys.append({"space", type("␣", " ")});
    keys.append({"comment", type("#", "#")});  // a comment is typed with the letters
    return keys;
}

}  // namespace

const QList<KeySection>& keySections() {
    static const QList<KeySection> sections{
        {"numbers", QT_TRANSLATE_NOOP("keypad", "Numbers"),
         {{"factorial", put("x!", "!")}, {"abs", shape("abs", Template::Abs, "abs")}, {"percent", put("%", "%")},
          {"percentages", tool("%…", "percentages")},
          {"mod", put("mod", "mod(", "mod")}, {"rem", put("rem", "rem(", "rem")},
          {"floormod", put("floormod", "floormod(", "floormod")}, {"npr", put("nPr", "nPr(", "nPr")}, {"ncr", put("nCr", "nCr(", "nCr")},
          {"gcd", put(QT_TRANSLATE_NOOP("keypad", "gcd"), QT_TRANSLATE_NOOP("keypad", "gcd("), "gcd")},
          {"lcm", put(QT_TRANSLATE_NOOP("keypad", "lcm"), QT_TRANSLATE_NOOP("keypad", "lcm("), "lcm")},
          {"comma", put(",", ", ")}}},
        {"hyperbolic", QT_TRANSLATE_NOOP("keypad", "Hyperbolic"),
         {{"sinh", put(QT_TRANSLATE_NOOP("keypad", "sinh"), QT_TRANSLATE_NOOP("keypad", "sinh("), "sinh")},
          {"cosh", put("cosh", "cosh(", "cosh")},
          {"tanh", put("tanh", "tanh(", "tanh")},
          {"asinh", put(QT_TRANSLATE_NOOP("keypad", "asinh"), QT_TRANSLATE_NOOP("keypad", "asinh("), "asinh")},
          {"acosh", put(QT_TRANSLATE_NOOP("keypad", "acosh"), QT_TRANSLATE_NOOP("keypad", "acosh("), "acosh")},
          {"atanh", put(QT_TRANSLATE_NOOP("keypad", "atanh"), QT_TRANSLATE_NOOP("keypad", "atanh("), "atanh")},
          {"sech", put("sech", "sech(", "sech")},
          {"csch", put("csch", "csch(", "csch")},
          {"coth", put("coth", "coth(", "coth")},
          {"asech", put("asech", "asech(", "asech")},
          {"acsch", put("acsch", "acsch(", "acsch")},
          {"acoth", put("acoth", "acoth(", "acoth")}}},
        {"trigonometry", QT_TRANSLATE_NOOP("keypad", "Trigonometry"),
         {{"asin", put(QT_TRANSLATE_NOOP("keypad", "asin"), QT_TRANSLATE_NOOP("keypad", "asin("), "asin")},
          {"acos", put(QT_TRANSLATE_NOOP("keypad", "acos"), QT_TRANSLATE_NOOP("keypad", "acos("), "acos")},
          {"atan", put(QT_TRANSLATE_NOOP("keypad", "atan"), QT_TRANSLATE_NOOP("keypad", "atan("), "atan")},
          {"sec", put("sec", "sec(", "sec")},
          {"csc", put("csc", "csc(", "csc")},
          {"cot", put("cot", "cot(", "cot")},
          {"asec", put("asec", "asec(", "asec")},
          {"acsc", put("acsc", "acsc(", "acsc")},
          {"acot", put("acot", "acot(", "acot")},
          {"atan2", put("atan2", "atan2(", "atan2")},
          {"hypot", put("hypot", "hypot(", "hypot")},
          {"sinc", put("sinc", "sinc(", "sinc")}}},
        {"powers", QT_TRANSLATE_NOOP("keypad", "Powers, roots and logs"),
         {{"cube", shape("x³", Template::Power, {}, "3")}, {"cbrt", shape("∛", Template::Cbrt, "cbrt")}, {"root", shape("ⁿ√", Template::Root, "root")},
          {"power10", shape("10ˣ", Template::Pow10, "exp10")}, {"exp", shape("eˣ", Template::Exp, "exp")}, {"log", put("log", "log(", "log")},
          {"log2", put("log2", "log2(", "log2")}, {"exp2", put("exp2", "exp2(", "exp2")}, {"sqrtpi", put("sqrtpi", "sqrtpi(", "sqrtpi")},
          {"sum", shape("Σ", Template::Sum, "sum")}, {"product", shape("Π", Template::Product, "product")},
          {"variable", put("x", "x")}}},
        {"rounding", QT_TRANSLATE_NOOP("keypad", "Rounding and parts"),
         {{"round", put("round", "round(", "round")},
          {"floor", put("floor", "floor(", "floor")},
          {"ceil", put("ceil", "ceil(", "ceil")},
          {"trunc", put("trunc", "trunc(", "trunc")},
          {"int", put("int", "int(", "int")},
          {"frac", put("frac", "frac(", "frac")},
          {"clip", put("clip", "clip(", "clip")},
          {"numerator", put("numerator", "numerator(", "numerator")},
          {"denominator", put("denominator", "denominator(", "denominator")},
          {"sgn", put("sgn", "sgn(", "sgn")}}},
        {"constants", QT_TRANSLATE_NOOP("keypad", "Constants"), {{"pi", put("π", "π", "pi")}, {"e", put("e", "e", "e")}}},
        {"statistics", QT_TRANSLATE_NOOP("keypad", "Statistics"), statisticsKeys()},
        {"showAs", QT_TRANSLATE_NOOP("keypad", "Show as"), QList<Key>{{"to", put("→", "→")}} + conversionKeys(showAsTargets())},
        {"special", QT_TRANSLATE_NOOP("keypad", "Special functions"),
         {{"gamma", put("gamma", "gamma(", "gamma")},
          {"lgamma", put("lgamma", "lgamma(", "lgamma")},
          {"beta", put("beta", "beta(", "beta")},
          {"digamma", put("digamma", "digamma(", "digamma")},
          {"erf", put("erf", "erf(", "erf")},
          {"erfc", put("erfc", "erfc(", "erfc")},
          {"erfinv", put("erfinv", "erfinv(", "erfinv")},
          {"erfcinv", put("erfcinv", "erfcinv(", "erfcinv")},
          {"gammap", put("gammap", "gammap(", "gammap")},
          {"gammaq", put("gammaq", "gammaq(", "gammaq")},
          {"igamma", put("igamma", "igamma(", "igamma")},
          {"gammainc", put("gammainc", "gammainc(", "gammainc")},
          {"betainc", put("betainc", "betainc(", "betainc")},
          {"betaincinv", put("betaincinv", "betaincinv(", "betaincinv")}}},
        {"variables", QT_TRANSLATE_NOOP("keypad", "Variables"),
         {{"varA", type("A", "A")}, {"varB", type("B", "B")}, {"varC", type("C", "C")}, {"varD", type("D", "D")},
          {"varE", type("E", "E")}, {"varF", type("F", "F")}, {"varY", type("y", "y")},
          {"store", act("STO", KeyAction::Store)}, {"assign", type(":=", ":=")}}},
        {"letters", QT_TRANSLATE_NOOP("keypad", "Letters"), letterKeys()},
    };
    return sections;
}

const QList<QPair<QString, QString>>& aliasKeys() {
    static const QList<QPair<QString, QString>> list{{"exp10", "power10"}, {"sq", "square"}};
    return list;
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

QString legend(const Face& face, bool decimalComma) {
    if (decimalComma && face.label == ".") return QStringLiteral(",");
    if (decimalComma && face.label == ",") return QStringLiteral(";");
    return translated(face.label);
}

const QList<QPair<QString, QStringList>>& alternates() {
    static const QList<QPair<QString, QStringList>> list{
        {"sin", {"asin", "sinh", "asinh", "csc", "acsc"}},
        {"cos", {"acos", "cosh", "acosh", "sec", "asec"}},
        {"tan", {"atan", "tanh", "atanh", "cot", "acot", "atan2"}},
        {"ln", {"log", "exp", "log2"}},
        {"logBase", {"log", "log2"}},
        {"square", {"cube"}},
        {"sqrt", {"cbrt", "root", "sqrtpi"}},
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
        {"%…", QT_TRANSLATE_NOOP("spoken", "percentages")},
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
        {"#", QT_TRANSLATE_NOOP("spoken", "comment")},
        {"_", QT_TRANSLATE_NOOP("spoken", "underscore")},
        {"→", QT_TRANSLATE_NOOP("spoken", "convert to")},
        {"Σ", QT_TRANSLATE_NOOP("spoken", "sum")},
        {"STO", QT_TRANSLATE_NOOP("spoken", "store")},
        {":=", QT_TRANSLATE_NOOP("spoken", "assign")},
        {"Π", QT_TRANSLATE_NOOP("spoken", "product")},
    };
    if (key.id.startsWith("to:"))  // a one-tap conversion: "convert to fraction"
        return QCoreApplication::translate("spoken", "convert to") + QLatin1Char(' ') + key.id.mid(3);
    const auto word = words.constFind(key.face.label);
    return word == words.constEnd() ? translated(key.face.label) : QCoreApplication::translate("spoken", *word);
}
