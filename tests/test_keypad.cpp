#include "keypad.hpp"

#include "constanttext.hpp"
#include "printers.hpp"

#include <calculate-core/calculate-core.hpp>

#include <gtest/gtest.h>

#include <QSet>
#include <QStringList>

#include <algorithm>

namespace {

QStringList labels(const QList<Key>& keys) {
    QStringList list;
    for (const Key& key : keys) list << key.face.label;
    return list;
}

QList<Key> everyKey() {
    QList<Key> all = cursorPad();
    for (const QList<Key>& row : keypad()) all += row;
    all += everyDirectKey();
    return all;
}

Key find(const QString& id) {
    for (const Key& key : everyKey())
        if (key.id == id) return key;
    return Key{};
}

}  // namespace

TEST(Keypad, TheRightPadHasOnlyWorkingKeys) {
    const QList<QList<Key>>& rows = keypad();
    ASSERT_EQ(rows.size(), 7);
    EXPECT_EQ(labels(rows[0]), QStringList({"(", ")", "□/□", "√□"}));
    EXPECT_EQ(labels(rows[1]), QStringList({"x²", "x^□", "(−)", "x⁻¹"}));
    EXPECT_EQ(labels(rows[2]), QStringList({"sin", "cos", "tan", "log□□", "ln", "M+"}));
    EXPECT_EQ(labels(rows[3]), QStringList({"7", "8", "9", "DEL", "AC"}));
    EXPECT_EQ(labels(rows[4]), QStringList({"4", "5", "6", "×", "÷"}));
    EXPECT_EQ(labels(rows[5]), QStringList({"1", "2", "3", "+", "−"}));
    EXPECT_EQ(labels(rows[6]), QStringList({"0", ".", "×10ˣ", "Ans", "="}));
    EXPECT_EQ(labels(cursorPad()), QStringList({"▲", "◄", "►", "▼"}));
}

TEST(Keypad, IdsAreUnique) {
    QSet<QString> ids;
    for (const Key& key : everyKey()) {
        EXPECT_FALSE(key.id.isEmpty()) << key.face.label.toStdString();
        EXPECT_FALSE(ids.contains(key.id)) << key.id.toStdString();
        ids.insert(key.id);
    }
}

TEST(Keypad, KeysInsertWhatTheyShow) {
    EXPECT_EQ(find("sin").face.insert, "sin(");
    EXPECT_EQ(find("fraction").face.shape, Template::Fraction);
    EXPECT_EQ(find("sqrt").face.shape, Template::Sqrt);
    EXPECT_EQ(find("square").face.shape, Template::Power);
    EXPECT_EQ(find("square").face.insert, "2");  // a power with its exponent filled in
    EXPECT_EQ(find("power").face.shape, Template::Power);
    EXPECT_EQ(find("power").face.insert, "");
    EXPECT_EQ(find("negative").face.insert, "-");
    EXPECT_EQ(find("reciprocal").face.insert, "−1");
    EXPECT_EQ(find("logBase").face.shape, Template::LogBase);
    for (const char* id : {"fraction", "sqrt", "square", "power", "reciprocal", "logBase", "cube", "cbrt", "root", "power10", "exp", "abs"})
        EXPECT_EQ(find(id).face.action, KeyAction::Template) << id;
    EXPECT_EQ(find("exponent").face.insert, "e");
    EXPECT_EQ(find("memoryAdd").face.action, KeyAction::MemoryAdd);
    EXPECT_EQ(find("clear").face.action, KeyAction::Clear);
    EXPECT_EQ(find("equals").face.action, KeyAction::Evaluate);
}

// Nothing that the calculator's SHIFT, ALPHA or OPTN used to reach is lost: every function has a key.
TEST(Keypad, EveryFunctionHasAKey) {
    QList<Face> faces;
    for (const Key& key : everyKey()) faces << key.face;
    const auto inserts = [&](const QString& text) {
        return std::any_of(faces.begin(), faces.end(), [&](const Face& f) { return f.action == KeyAction::Insert && f.insert == text; });
    };
    for (const char* text : {"asin(", "acos(", "atan(", "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(", "log(",
                             "!", "%", "mod(", "nPr(", "nCr(", "gcd(", "lcm(", ", ", "π", "e", "M", "Ans"})
        EXPECT_TRUE(inserts(text)) << text;
    for (Template shape : {Template::Fraction, Template::Sqrt, Template::Cbrt, Template::Root, Template::Power, Template::Exp,
                           Template::Pow10, Template::LogBase, Template::Abs})
        EXPECT_TRUE(std::any_of(faces.begin(), faces.end(), [&](const Face& f) { return f.shape == shape; }));
    for (KeyAction action : {KeyAction::MemoryAdd, KeyAction::MemorySubtract, KeyAction::MemoryClear, KeyAction::MemoryStore})
        EXPECT_TRUE(std::any_of(faces.begin(), faces.end(), [&](const Face& f) { return f.action == action; }));
}

TEST(Keypad, AvailabilityFollowsTheEngine) {
    EXPECT_TRUE(available(find("sin").face, false));
    EXPECT_FALSE(available(find("sin").face, true));
    EXPECT_FALSE(available(find("pi").face, true));
    EXPECT_FALSE(available(find("exp").face, true));
    EXPECT_TRUE(available(find("sqrt").face, true));
    EXPECT_TRUE(available(find("7").face, true));
    EXPECT_TRUE(available(find("factorial").face, true));
}

namespace {

const KeySection& section(const QString& id) {
    static const KeySection none{};
    for (const KeySection& s : keySections())
        if (s.id == id) return s;
    return none;
}

QStringList sectionLabels(const QString& id) { return labels(section(id).keys); }

}  // namespace

TEST(Keypad, EveryOtherKeyHasAHomeSection) {
    QStringList ids, titles;
    for (const KeySection& s : keySections()) {
        ids << s.id;
        titles << s.title;
    }
    EXPECT_EQ(ids, QStringList({"numbers", "hyperbolic", "trigonometry", "powers", "rounding", "constants", "statistics", "showAs", "special", "variables", "letters"}));
    EXPECT_EQ(titles, QStringList({"Numbers", "Hyperbolic", "Trigonometry", "Powers, roots and logs", "Rounding and parts", "Constants", "Statistics", "Show as", "Special functions", "Variables", "Letters"}));
    EXPECT_EQ(sectionLabels("numbers"), QStringList({"x!", "abs", "%", "%…", "mod", "rem", "floormod", "nPr", "nCr", "gcd", "lcm", ","}));
    EXPECT_EQ(sectionLabels("hyperbolic"), QStringList({"sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "sech", "csch",
                                                        "coth", "asech", "acsch", "acoth"}));
    EXPECT_EQ(sectionLabels("trigonometry"),
              QStringList({"asin", "acos", "atan", "sec", "csc", "cot", "asec", "acsc", "acot", "atan2", "hypot", "sinc"}));
    EXPECT_EQ(sectionLabels("powers"), QStringList({"x³", "∛", "ⁿ√", "10ˣ", "eˣ", "log", "log2", "exp2", "sqrtpi", "Σ", "Π", "x"}));
    EXPECT_EQ(sectionLabels("constants"), QStringList({"π", "e", "φ", "τ", "γ", "catalan", "apery", "√2", "plastic", "Ω", "±",
                                                       "uncertainty", "errorPart"}));
    EXPECT_EQ(labels(memoryKeys()), QStringList({"MS", "M", "M−", "MC", "↶", "↷"}));
}

TEST(Keypad, CommonStartsWithTheKeysUsedMost) {
    EXPECT_EQ(defaultCommon(),
              QStringList({"asin", "acos", "atan", "pi", "e", "factorial", "power10", "exp", "cube", "cbrt", "root", "abs"}));
    for (const QString& id : defaultCommon()) {
        bool home = false;
        for (const KeySection& s : keySections())
            for (const Key& key : s.keys) home = home || key.id == id;
        EXPECT_TRUE(home) << id.toStdString();  // taken out of Common, a key stays reachable
    }
    EXPECT_EQ(directKey("factorial").face.insert, "!");
    EXPECT_EQ(directKey("memoryClear").face.action, KeyAction::MemoryClear);
    EXPECT_TRUE(directKey("nothing").id.isEmpty());
}

TEST(Keypad, EverySectionHasAnIdATitleAndKeys) {
    QSet<QString> ids;
    QList<Key> inOrder = memoryKeys();
    for (const KeySection& s : keySections()) {
        EXPECT_FALSE(s.id.isEmpty());
        EXPECT_FALSE(ids.contains(s.id)) << s.id.toStdString();
        ids.insert(s.id);
        EXPECT_FALSE(s.title.isEmpty()) << s.id.toStdString();
        EXPECT_FALSE(s.keys.isEmpty()) << s.id.toStdString();  // a section appears with its first key
        inOrder += s.keys;
    }
    EXPECT_EQ(labels(everyDirectKey()), labels(inOrder));  // Memory and editing, then section by section
}

TEST(Keypad, SectionsKeepTheirOrder) {
    const QStringList order{"numbers", "hyperbolic", "trigonometry", "powers", "rounding", "constants", "statistics",
                            "showAs", "programming", "special", "variables", "letters"};
    int last = -1;
    for (const KeySection& s : keySections()) {
        const int at = order.indexOf(s.id);
        EXPECT_GT(at, last) << s.id.toStdString();  // a known section, after the one before it
        last = at;
    }
}

TEST(Keypad, TheStatisticsFunctionsAreKeysToo) {
    QStringList ids;
    for (const Key& key : statisticsKeys()) {
        ids << key.id;
        EXPECT_EQ(key.face.function, key.id);
        EXPECT_EQ(key.face.insert, key.id + "(");
    }
    EXPECT_EQ(ids, QStringList({"mean", "median", "var", "stdev", "varp", "stdevp"}));
}

// The rule that keeps the keyboards complete: a function the engine offers and no key reaches fails here.
TEST(Keypad, EveryEngineFunctionHasAKey) {
    QSet<QString> onKeys;
    for (const Key& key : everyKey()) onKeys.insert(key.face.function);
    for (const Key& key : statisticsKeys()) onKeys.insert(key.face.function);
    QHash<QString, QString> twins;
    for (const auto& [alias, id] : aliasKeys()) twins.insert(alias, id);
    for (const calculate_core::FunctionDescription& f : calculate_core::functions()) {
        const QString name = QString::fromStdString(f.name);
        EXPECT_TRUE(onKeys.contains(name) || !find(twins.value(name)).id.isEmpty()) << f.name;
    }
}

TEST(Keypad, AStatisticsSectionTypesTheStatisticsIntoExpressions) {
    EXPECT_EQ(section("statistics").title, "Statistics");
    EXPECT_EQ(sectionLabels("statistics"), QStringList({"mean", "median", "var", "stdev", "varp", "stdevp"}));
    EXPECT_EQ(directKey("stdevp").face.insert, "stdevp(");
}

TEST(Keypad, TheLettersSectionHasTheAlphabet) {
    EXPECT_EQ(keySections().last().id, "letters");
    EXPECT_EQ(section("letters").title, "Letters");
    QStringList alphabet;
    for (char c = 'a'; c <= 'z'; ++c) alphabet << QString(QChar(c));
    EXPECT_EQ(sectionLabels("letters"), alphabet + QStringList({"⇧", "_", "␣", "#"}));
    EXPECT_EQ(directKey("letterA").face.action, KeyAction::Type);
    EXPECT_EQ(directKey("letterA").face.insert, "a");
    EXPECT_EQ(directKey("shift").face.action, KeyAction::Shift);
    EXPECT_EQ(directKey("space").face.insert, " ");
    EXPECT_EQ(find("open").face.action, KeyAction::Type);  // ( follows the typing rules: it can end a name
}

TEST(Keypad, UndoAndRedoAreKeys) {
    const QStringList memory = labels(memoryKeys());
    EXPECT_EQ(memory.mid(memory.size() - 2), QStringList({"↶", "↷"}));
    EXPECT_EQ(directKey("undo").face.action, KeyAction::Undo);
    EXPECT_EQ(directKey("redo").face.action, KeyAction::Redo);
}

TEST(Keypad, TheSearchListsEveryKeyUnderItsSection) {
    QStringList groups;
    QSet<QString> functions;
    for (const SearchEntry& e : searchEntries()) {
        if (groups.isEmpty() || groups.last() != e.group) groups << e.group;
        functions.insert(e.face.function);
    }
    QStringList titles;
    int keys = 0;
    for (const KeySection& s : keySections())
        if (s.id != "letters") {  // letters are typing, not something to find
            titles << s.title;
            keys += s.keys.size();
        }
    EXPECT_EQ(groups, titles + QStringList({"Main keys"}));  // by section, in order, then the main pad's functions
    EXPECT_GE(searchEntries().size(), keys);
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        EXPECT_TRUE(functions.contains(QString::fromStdString(f.name))) << f.name;  // every function can be found
    EXPECT_TRUE(extraSearchEntries().isEmpty());  // until Plan 3 adds the constants without keys
}

TEST(Keypad, SearchMatchesNamesWhatTheyTypeAndHeadings) {
    SearchEntry asinh;
    for (const SearchEntry& e : searchEntries())
        if (e.face.function == "asinh") asinh = e;
    ASSERT_EQ(asinh.group, "Hyperbolic");
    EXPECT_TRUE(searchMatches(asinh, ""));        // an empty box lists everything
    EXPECT_TRUE(searchMatches(asinh, "ASIN"));    // the legend, ignoring case
    EXPECT_TRUE(searchMatches(asinh, "inh("));    // what it types
    EXPECT_TRUE(searchMatches(asinh, "hyperb"));  // its heading
    EXPECT_FALSE(searchMatches(asinh, "gcd"));
    const SearchEntry pi{"Constants", Face{"π", "π", "pi"}, "the ratio of a circle's circumference to its diameter"};
    EXPECT_TRUE(searchMatches(pi, "circle"));  // its description
    EXPECT_TRUE(searchMatches(pi, "PI"));      // its function
}

// Every piece of the language that is not a function name can be entered with keys alone.
TEST(Keypad, EverySyntaxElementHasAKey) {
    QSet<QString> entered;  // what some key puts in the input, directly or through the typing rules
    for (const Key& key : everyKey())
        if (key.face.action == KeyAction::Insert || key.face.action == KeyAction::Type) entered.insert(key.face.insert);
    QStringList syntax{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "e", "+", "−", "×", "÷", "(", ")",
                       ", ", "!", "%", "-", "π", "Ans", "M", "_", " ", "#", "→", ":=", "±"};
    for (char c = 'a'; c <= 'z'; ++c) syntax << QString(QChar(c));  // capitals through ⇧
    for (const QString& piece : syntax) EXPECT_TRUE(entered.contains(piece)) << piece.toStdString();
    QSet<Template> shapes;
    for (const Key& key : everyKey()) shapes.insert(key.face.shape);
    for (Template t : {Template::Fraction, Template::Sqrt, Template::Cbrt, Template::Root, Template::Power, Template::Exp,
                       Template::Pow10, Template::LogBase, Template::Abs, Template::Sum, Template::Product})
        EXPECT_TRUE(shapes.contains(t)) << static_cast<int>(t);
    QSet<KeyAction> actions;
    for (const Key& key : everyKey()) actions.insert(key.face.action);
    for (KeyAction a : {KeyAction::Clear, KeyAction::Backspace, KeyAction::Evaluate, KeyAction::Left, KeyAction::Right,
                        KeyAction::Up, KeyAction::Down, KeyAction::Undo, KeyAction::Redo, KeyAction::Shift, KeyAction::Store})
        EXPECT_TRUE(actions.contains(a)) << static_cast<int>(a);
}

TEST(Keypad, CommonHoldsSectionKeysOnly) {
    EXPECT_EQ(commonLimit, 12);
    EXPECT_TRUE(canBeCommon(defaultCommon()));
    EXPECT_TRUE(canBeCommon({}));               // the user may empty it
    EXPECT_TRUE(canBeCommon({"sinh", "gcd"}));
    EXPECT_FALSE(canBeCommon({"sinh", "sinh"}));  // once each
    EXPECT_FALSE(canBeCommon({"7"}));             // the main pad is always in view already
    EXPECT_FALSE(canBeCommon({"memoryClear"}));   // so is Memory and editing
    EXPECT_FALSE(canBeCommon({"nothing"}));
    QStringList thirteen = defaultCommon();
    thirteen << "sinh";
    EXPECT_FALSE(canBeCommon(thirteen));
}

TEST(Keypad, AlternatesAreKeysOfTheLeftKeyboard) {
    ASSERT_FALSE(alternates().isEmpty());
    for (const auto& [id, others] : alternates()) {
        EXPECT_FALSE(find(id).id.isEmpty()) << id.toStdString();
        for (const QString& other : others) EXPECT_FALSE(find(other).id.isEmpty()) << other.toStdString();
    }
}

TEST(Keypad, APercentagesKeyOpensTheTool) {
    EXPECT_EQ(directKey("percentages").face.action, KeyAction::Tool);
    EXPECT_EQ(directKey("percentages").face.opens, "percentages");
    const QStringList numbers = sectionLabels("numbers");
    EXPECT_EQ(numbers.indexOf("%…"), numbers.indexOf("%") + 1);  // next to %
}

TEST(Keypad, TheSeparatorKeysFollowTheDecimalComma) {
    Face point;
    for (const QList<Key>& row : keypad())
        for (const Key& key : row)
            if (key.id == "point") point = key.face;
    ASSERT_EQ(point.label, ".");
    EXPECT_EQ(legend(point, false), ".");
    EXPECT_EQ(legend(point, true), ",");                     // the decimal separator
    EXPECT_EQ(legend(directKey("comma").face, false), ",");
    EXPECT_EQ(legend(directKey("comma").face, true), ";");   // the argument separator
    EXPECT_EQ(legend(directKey("sinh").face, true), translated("sinh"));
}

TEST(Keypad, ACommentKeyWithTheLetters) {
    const QStringList letters = sectionLabels("letters");
    EXPECT_EQ(letters.mid(letters.size() - 4), QStringList({"⇧", "_", "␣", "#"}));  // a comment is typed with letters
    EXPECT_EQ(directKey("comment").face.action, KeyAction::Type);
    EXPECT_EQ(directKey("comment").face.insert, "#");
}

TEST(Keypad, TheCommentKeyHasASpokenName) {
    EXPECT_EQ(spokenName(directKey("comment")), "comment");  // not the bare sign
}

TEST(Keypad, TheConversionKeys) {
    EXPECT_EQ(section("showAs").title, "Show as");
    EXPECT_EQ(sectionLabels("showAs").first(), "→");
    EXPECT_EQ(directKey("to").face.insert, "→");
    EXPECT_EQ(directKey("to:fraction").face.insert, "→fraction");
}

// Every target of → has a one-tap key; it is generated, so a new target needs no new line.
TEST(Keypad, EveryConversionTargetHasAKey) {
    for (const calculate_core::TargetDescription& target : calculate_core::conversionTargets()) {
        const Key key = directKey("to:" + QString::fromStdString(target.name));
        EXPECT_FALSE(key.id.isEmpty()) << target.name;
        EXPECT_TRUE(key.face.insert.startsWith("→")) << target.name;
    }
}

TEST(Keypad, ConversionKeysHaveSpokenNames) {
    EXPECT_EQ(spokenName(directKey("to")), "convert to");
    EXPECT_EQ(spokenName(directKey("to:fraction")), "convert to fraction");
}

TEST(Keypad, SumsAndProductsHaveKeys) {
    const QStringList powers = sectionLabels("powers");
    EXPECT_EQ(powers.mid(powers.size() - 3), QStringList({"Σ", "Π", "x"}));
    EXPECT_EQ(directKey("sum").face.shape, Template::Sum);
    EXPECT_EQ(directKey("product").face.shape, Template::Product);
    EXPECT_EQ(directKey("variable").face.insert, "x");
}

TEST(Keypad, SumAndProductKeysHaveSpokenNames) {
    EXPECT_EQ(spokenName(directKey("sum")), "sum");  // not the bare sign
    EXPECT_EQ(spokenName(directKey("product")), "product");
}

TEST(Keypad, TheVariablesSection) {
    EXPECT_EQ(section("variables").title, "Variables");
    EXPECT_EQ(sectionLabels("variables"), QStringList({"A", "B", "C", "D", "E", "F", "y", "STO", ":="}));
    EXPECT_EQ(directKey("store").face.action, KeyAction::Store);
    EXPECT_EQ(directKey("assign").face.insert, ":=");
    EXPECT_EQ(directKey("varA").face.insert, "A");
}

TEST(Keypad, TheElementaryFunctionsHaveKeys) {
    EXPECT_EQ(sectionLabels("trigonometry"),
              QStringList({"asin", "acos", "atan", "sec", "csc", "cot", "asec", "acsc", "acot", "atan2", "hypot", "sinc"}));
    EXPECT_EQ(sectionLabels("hyperbolic"), QStringList({"sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "sech", "csch",
                                                        "coth", "asech", "acsch", "acoth"}));
    const QStringList powers = sectionLabels("powers");
    EXPECT_EQ(powers.mid(powers.indexOf("log"), 4), QStringList({"log", "log2", "exp2", "sqrtpi"}));
    EXPECT_EQ(section("rounding").title, "Rounding and parts");
    EXPECT_EQ(sectionLabels("rounding"), QStringList({"round", "floor", "ceil", "trunc", "int", "frac", "clip", "numerator",
                                                      "denominator", "sgn"}));
    for (const char* name : {"sec", "acoth", "atan2", "hypot", "log2", "sqrtpi", "round", "frac", "denominator", "sgn"}) {
        EXPECT_EQ(directKey(name).face.insert, QString(name) + "(") << name;
        EXPECT_EQ(directKey(name).face.function, name) << name;
    }
}

TEST(Keypad, AnAliasUsesItsTwinsKey) {
    ASSERT_FALSE(aliasKeys().isEmpty());
    for (const auto& [alias, id] : aliasKeys()) EXPECT_FALSE(find(id).id.isEmpty()) << alias.toStdString();
}

TEST(Keypad, TheSpecialFunctionsHaveKeys) {
    EXPECT_EQ(section("special").title, "Special functions");
    EXPECT_EQ(sectionLabels("special"),
              QStringList({"gamma", "lgamma", "beta", "digamma", "erf", "erfc", "erfinv", "erfcinv", "gammap", "gammaq",
                           "igamma", "gammainc", "betainc", "betaincinv"}));
    for (const char* name : {"gamma", "erfcinv", "gammainc", "betaincinv"}) EXPECT_EQ(directKey(name).face.function, name) << name;
}

TEST(Keypad, AKeyWithoutAFunctionIsDescribedByItsSpokenName) {
    EXPECT_EQ(keyTip(directKey("undo")), "Undo");
    EXPECT_EQ(keyName(directKey("undo")), "undo");
}

TEST(Keypad, SearchEntriesCarryTheirTitles) {
    SearchEntry asin;
    for (const SearchEntry& e : searchEntries()) {
        if (!e.face.function.isEmpty()) EXPECT_FALSE(e.title.isEmpty()) << e.face.label.toStdString();
        if (e.face.function == "asin") asin = e;
    }
    EXPECT_EQ(asin.title, "Inverse sine");
    EXPECT_TRUE(searchMatches(asin, "inverse sine"));  // found by what it does
}

TEST(Keypad, TheConstantsSection) {
    EXPECT_EQ(sectionLabels("constants"), QStringList({"π", "e", "φ", "τ", "γ", "catalan", "apery", "√2", "plastic", "Ω", "±",
                                                       "uncertainty", "errorPart"}));
    EXPECT_EQ(directKey("plusMinus").face.insert, "±");
    EXPECT_EQ(directKey("uncertainty").face.insert, "uncertainty(");
    EXPECT_EQ(directKey("errorPart").face.function, "errorPart");
    EXPECT_EQ(directKey("golden").face.insert, "φ");
    EXPECT_EQ(directKey("omega").face.insert, "omega");
    for (const SearchEntry& e : searchEntries())
        if (e.face.insert == "φ") EXPECT_EQ(e.title, constantTitle("phi"));  // found by what it is
}
