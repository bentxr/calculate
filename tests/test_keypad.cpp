#include "keypad.hpp"

#include "printers.hpp"

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
    for (const KeyGroup& group : directKeys()) all += group.keys;
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
    EXPECT_EQ(find("fraction").face.insert, "/");
    EXPECT_EQ(find("sqrt").face.insert, "√(");
    EXPECT_EQ(find("square").face.insert, "²");
    EXPECT_EQ(find("power").face.insert, "^");
    EXPECT_EQ(find("negative").face.insert, "-");
    EXPECT_EQ(find("reciprocal").face.insert, "^-1");
    EXPECT_EQ(find("logBase").face.insert, "log(");
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
    for (const char* text : {"asin(", "acos(", "atan(", "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(", "³", "∛(",
                             "root(", "10^", "exp(", "log(", "!", "abs(", "%", "mod(", "nPr(", "nCr(", "gcd(", "lcm(", ", ",
                             "π", "e", "M", "Ans"})
        EXPECT_TRUE(inserts(text)) << text;
    for (KeyAction action : {KeyAction::MemoryAdd, KeyAction::MemorySubtract, KeyAction::MemoryClear})
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

TEST(Keypad, DirectKeysGroupTheOtherFunctions) {
    QStringList titles, all;
    for (const KeyGroup& group : directKeys()) {
        titles << group.title;
        all << labels(group.keys);
    }
    EXPECT_EQ(titles, QStringList({"Trigonometry", "Hyperbolic", "Powers and roots", "Numbers", "Constants and memory"}));
    EXPECT_EQ(all, QStringList({"asin", "acos", "atan", "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "x³", "∛", "ⁿ√",
                                "10ˣ", "eˣ", "log", "x!", "abs", "%", "mod", "nPr", "nCr", "gcd", "lcm", ",", "π", "e",
                                "M−", "M", "MC"}));
}
