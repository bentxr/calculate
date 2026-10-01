#include "keypad.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

#include <QSet>
#include <QStringList>

namespace {

QStringList labels(const QList<Key>& keys) {
    QStringList list;
    for (const Key& key : keys) list << key.main.label;
    return list;
}

Key find(const QString& id) {
    for (const QList<Key>& row : keypad())
        for (const Key& key : row)
            if (key.id == id) return key;
    for (const Key& key : cursorPad())
        if (key.id == id) return key;
    return Key{};
}

}  // namespace

TEST(Keypad, FollowsTheScientificLayout) {
    const QList<QList<Key>>& rows = keypad();
    ASSERT_EQ(rows.size(), 9);
    EXPECT_EQ(labels(rows[0]), QStringList({"SHIFT", "ALPHA", "MENU", "ON"}));
    EXPECT_EQ(labels(rows[1]), QStringList({"OPTN", "CALC", "∫□", "x"}));
    EXPECT_EQ(labels(rows[2]), QStringList({"□/□", "√□", "x²", "x^□", "log□□", "ln"}));
    EXPECT_EQ(labels(rows[3]), QStringList({"(−)", "°'\"", "x⁻¹", "sin", "cos", "tan"}));
    EXPECT_EQ(labels(rows[4]), QStringList({"STO", "ENG", "(", ")", "S⇔D", "M+"}));
    EXPECT_EQ(labels(rows[5]), QStringList({"7", "8", "9", "DEL", "AC"}));
    EXPECT_EQ(labels(rows[6]), QStringList({"4", "5", "6", "×", "÷"}));
    EXPECT_EQ(labels(rows[7]), QStringList({"1", "2", "3", "+", "−"}));
    EXPECT_EQ(labels(rows[8]), QStringList({"0", ".", "×10ˣ", "Ans", "="}));
    EXPECT_EQ(labels(cursorPad()), QStringList({"▲", "◄", "►", "▼"}));
}

TEST(Keypad, IdsAreUnique) {
    QSet<QString> ids;
    QList<Key> all = cursorPad();
    for (const QList<Key>& row : keypad()) all += row;
    for (const Key& key : all) {
        EXPECT_FALSE(key.id.isEmpty()) << key.main.label.toStdString();
        EXPECT_FALSE(ids.contains(key.id)) << key.id.toStdString();
        ids.insert(key.id);
    }
}

TEST(Keypad, ShiftAndAlphaFollowTheLegends) {
    EXPECT_EQ(find("sin").main.insert, "sin(");
    EXPECT_EQ(find("sin").shift.insert, "asin(");
    EXPECT_EQ(find("sqrt").shift.insert, "∛(");
    EXPECT_EQ(find("square").shift.insert, "³");
    EXPECT_EQ(find("logBase").shift.insert, "10^");
    EXPECT_EQ(find("ln").shift.insert, "exp(");
    EXPECT_EQ(find("negative").shift.insert, "log(");
    EXPECT_EQ(find("reciprocal").shift.insert, "!");
    EXPECT_EQ(find("open").shift.insert, "abs(");
    EXPECT_EQ(find("close").shift.insert, ", ");
    EXPECT_EQ(find("ans").shift.insert, "%");
    EXPECT_EQ(find("exponent").shift.insert, "π");
    EXPECT_EQ(find("exponent").alpha.insert, "e");
    EXPECT_EQ(find("multiply").shift.insert, "nPr(");
    EXPECT_EQ(find("divide").shift.insert, "nCr(");
    EXPECT_EQ(find("multiply").alpha.insert, "gcd(");
    EXPECT_EQ(find("divide").alpha.insert, "lcm(");
    EXPECT_EQ(find("memoryAdd").shift.action, KeyAction::MemorySubtract);
    EXPECT_EQ(find("memoryAdd").alpha.insert, "M");
    EXPECT_EQ(find("menu").shift.action, KeyAction::Config);
    EXPECT_EQ(find("calc").main.action, KeyAction::Unavailable);
    EXPECT_EQ(find("calc").shift.label, "SOLVE");
}

TEST(Keypad, AvailabilityFollowsTheEngine) {
    EXPECT_TRUE(available(find("sin").main, false));
    EXPECT_FALSE(available(find("sin").main, true));
    EXPECT_FALSE(available(find("exponent").shift, true));  // π
    EXPECT_FALSE(available(find("ln").shift, true));        // eˣ
    EXPECT_TRUE(available(find("sqrt").main, true));
    EXPECT_TRUE(available(find("7").main, true));
    EXPECT_TRUE(available(find("reciprocal").shift, true));  // x!
    EXPECT_FALSE(available(find("calc").main, false));
    EXPECT_FALSE(available(find("7").alpha, false));  // no legend
}
