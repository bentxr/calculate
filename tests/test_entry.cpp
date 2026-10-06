#include "entry.hpp"

#include "printers.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QStringList>

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

namespace {

using Path = std::vector<std::pair<int, int>>;

Entry fraction(std::initializer_list<const char*> numerator, std::initializer_list<const char*> denominator) {
    Entry e;
    e.insertTemplate(Template::Fraction);
    type(e, numerator);
    e.right();
    type(e, denominator);
    return e;
}

}  // namespace

TEST(Entry, LeftAndRightWalkThroughTheBoxes) {
    Entry e = fraction({"1"}, {"3"});
    e.right();  // out, after the fraction
    EXPECT_EQ(e.path(), Path{});
    e.left();  // into the denominator, at its end
    EXPECT_EQ(e.path(), (Path{{0, 1}}));
    EXPECT_EQ(e.cursor(), 1);
    e.left();
    e.left();  // from the denominator's start to the numerator's end
    EXPECT_EQ(e.path(), (Path{{0, 0}}));
    EXPECT_EQ(e.cursor(), 1);
    e.left();
    e.left();  // out, before the fraction
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.cursor(), 0);
}

TEST(Entry, UpAndDownMoveBetweenNumeratorAndDenominator) {
    Entry e = fraction({"1", "2"}, {});
    e.left();  // back in the numerator, at its end
    EXPECT_TRUE(e.down());
    EXPECT_EQ(e.path(), (Path{{0, 1}}));
    type(e, {"5"});
    EXPECT_TRUE(e.up());
    EXPECT_EQ(e.path(), (Path{{0, 0}}));
    EXPECT_TRUE(e.up());  // inside a fraction ▲ never replays the history
    EXPECT_EQ(e.text(), "((12)/(5))");
    Entry plain;
    type(plain, {"1"});
    EXPECT_FALSE(plain.up());  // outside a fraction: the history's turn
    EXPECT_FALSE(plain.down());
}

TEST(Entry, DeleteAtTheStartOfABoxUnwrapsTheTemplate) {
    Entry e;
    type(e, {"2", "+"});
    e.insertTemplate(Template::Sqrt);
    type(e, {"9"});
    e.left();       // the start of the radicand
    e.backspace();  // the √ goes, the 9 stays
    EXPECT_EQ(e.text(), "2+9");
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.cursor(), 2);
    Entry f = fraction({"1"}, {"3"});
    f.left();       // the start of the denominator
    f.backspace();  // a later box: back to the end of the previous one
    EXPECT_EQ(f.path(), (Path{{0, 0}}));
    EXPECT_EQ(f.cursor(), 1);
    f.left();
    f.backspace();  // the first box: the fraction goes, its parts stay, divided
    EXPECT_EQ(f.text(), "1÷3");
}

TEST(Entry, HomeEndAndDeleteForward) {
    Entry e;
    type(e, {"1", "2", "3"});
    e.home();
    EXPECT_EQ(e.cursor(), 0);
    e.deleteForward();
    EXPECT_EQ(e.text(), "23");
    e.end();
    EXPECT_EQ(e.cursor(), 2);
    e.deleteForward();  // nothing after the cursor
    EXPECT_EQ(e.text(), "23");
    Entry f = fraction({"1"}, {"3"});  // the cursor is in the denominator
    f.home();                           // the start of the whole input, not of the box
    EXPECT_EQ(f.path(), Path{});
    EXPECT_EQ(f.cursor(), 0);
    f.deleteForward();  // a template goes whole, like a piece
    EXPECT_TRUE(f.isEmpty());
}

TEST(Entry, APositionCanBeReadAndSet) {
    Entry e = fraction({"1", "2"}, {"3"});
    const Position inNumerator{{{0, 0}}, 1};
    e.setPosition(inNumerator);
    EXPECT_TRUE(e.position() == inNumerator);
    type(e, {"5"});
    EXPECT_EQ(e.text(), "((152)/(3))");
    e.setPosition(Position{{}, 9});  // past the end of the row: kept inside it
    EXPECT_EQ(e.cursor(), 1);
}

TEST(Entry, EntriesCompareByContentAndCursor) {
    Entry a, b;
    type(a, {"1"});
    type(b, {"1"});
    EXPECT_TRUE(a == b);
    b.left();
    EXPECT_FALSE(a == b);
    Entry c;
    c.setRoot({Item{Template::Text, "1", {}}});
    EXPECT_TRUE(a == c);  // setRoot puts the cursor at the end
}
