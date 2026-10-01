#include "entry.hpp"

#include "printers.hpp"

#include <calculate-core/calculate-core.hpp>

#include <gtest/gtest.h>

TEST(Entry, KeysInsertPiecesThatDeleteWhole) {
    Entry e;
    for (const char* piece : {"sin(", "3", "0", ")"}) e.insert(piece);
    EXPECT_EQ(e.text(), "sin(30)");
    e.backspace();
    EXPECT_EQ(e.text(), "sin(30");
    e.left();
    e.left();
    e.backspace();  // removes "sin(" in one go, as on the calculator
    EXPECT_EQ(e.text(), "30");
    EXPECT_EQ(e.cursor(), 0);
    e.backspace();  // nothing before the cursor
    EXPECT_EQ(e.text(), "30");
}

TEST(Entry, InsertsAtTheCursor) {
    Entry e;
    e.insert("1");
    e.insert("2");
    e.left();
    e.insert("+");
    EXPECT_EQ(e.text(), "1+2");
    e.right();
    e.right();  // already at the end
    EXPECT_EQ(e.cursor(), 3);
    e.left();
    e.left();
    e.left();
    e.left();  // already at the start
    EXPECT_EQ(e.cursor(), 0);
}

TEST(Entry, SetTextSplitsIntoPieces) {
    Entry e;
    e.setText("sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(e.pieces(), QStringList({"sin(", "1", "e", "1", "0", ")", "+", "Ans", "×", "nCr(", "5", ", ", "2", ")"}));
    EXPECT_EQ(e.text(), "sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(e.cursor(), 14);
    e.clear();
    EXPECT_TRUE(e.isEmpty());
    EXPECT_EQ(e.cursor(), 0);
}

namespace {

// The value the engine gives for the entry's text, as the screen writes it.
QString value(const Entry& e, calculate_core::NumberType type = calculate_core::NumberType::Double) {
    calculate_core::Options o;
    o.type = type;
    const calculate_core::Result r = calculate_core::evaluate(e.text().toStdString(), o);
    if (r.error) return QString::fromStdString(r.error->message);
    if (r.exact) return QString::fromStdString(r.exact->numerator + "/" + r.exact->denominator);
    return QString::fromStdString(r.value.digits) + "e" + QString::number(r.value.exponent10);
}

// Types `pieces` into the box the cursor is in.
void type(Entry& e, std::initializer_list<const char*> pieces) {
    for (const char* p : pieces) e.insert(QString::fromUtf8(p));
}

}  // namespace

TEST(Entry, AFractionHasANumeratorAndADenominator) {
    Entry e;
    e.insertTemplate(Template::Fraction);  // the cursor goes into the numerator
    type(e, {"1"});
    e.right();  // to the denominator
    type(e, {"3"});
    e.right();  // out of the fraction
    type(e, {"+", "1"});
    EXPECT_EQ(e.text(), "((1)/(3))+1");
    EXPECT_EQ(value(e, calculate_core::NumberType::Exact), "4/3");
}

TEST(Entry, EveryTemplateBecomesEngineText) {
    const auto one = [](Template t, std::initializer_list<const char*> first, std::initializer_list<const char*> second = {}) {
        Entry e;
        e.insertTemplate(t);
        type(e, first);
        if (second.size()) {
            e.right();
            type(e, second);
        }
        return e;
    };
    EXPECT_EQ(one(Template::Sqrt, {"4"}).text(), "√(4)");
    EXPECT_EQ(one(Template::Cbrt, {"8"}).text(), "∛(8)");
    EXPECT_EQ(one(Template::Root, {"5"}, {"3", "2"}).text(), "root(32, 5)");  // index first, as on the calculator
    EXPECT_EQ(one(Template::Exp, {"1"}).text(), "exp(1)");
    EXPECT_EQ(one(Template::Pow10, {"3"}).text(), "(10^(3))");
    EXPECT_EQ(one(Template::LogBase, {"2"}, {"8"}).text(), "log(8, 2)");     // the base first on screen, second for the engine
    EXPECT_EQ(one(Template::Abs, {"-", "3"}).text(), "abs(-3)");
    Entry power;
    type(power, {"2"});
    power.insertTemplate(Template::Power);
    type(power, {"1", "0"});
    EXPECT_EQ(power.text(), "2^(10)");
    EXPECT_EQ(value(power, calculate_core::NumberType::Exact), "1024/1");
    EXPECT_EQ(value(one(Template::Root, {"5"}, {"3", "2"}), calculate_core::NumberType::Exact), "2/1");
}

TEST(Entry, ATemplateAfterADigitNeverMergesWithIt) {
    Entry e;
    type(e, {"5"});
    e.insertTemplate(Template::Pow10);
    type(e, {"3"});
    EXPECT_EQ(e.text(), "5(10^(3))");  // an error for the engine (no implicit multiplication), never 510^3
    EXPECT_TRUE(value(e).startsWith("Missing operator"));
}
