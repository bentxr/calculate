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

TEST(Entry, ShiftArrowsSelectWholeItems) {
    Entry e;
    type(e, {"1", "+", "2"});
    EXPECT_FALSE(e.hasSelection());
    e.extendLeft();
    e.extendLeft();
    EXPECT_TRUE(e.hasSelection());
    EXPECT_EQ(e.selectedText(), "+2");
    e.left();  // a plain move ends the selection, at its start
    EXPECT_FALSE(e.hasSelection());
    EXPECT_EQ(e.cursor(), 1);
}

TEST(Entry, SelectingPastABoxTakesTheWholeTemplate) {
    Entry e;
    type(e, {"2", "+"});
    e.insertTemplate(Template::Sqrt);
    type(e, {"9"});
    e.extendLeft();  // the 9
    EXPECT_EQ(e.selectedText(), "9");
    e.extendLeft();  // past the box: the whole √
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.selectedText(), "√(9)");
    e.extendLeft();
    EXPECT_EQ(e.selectedText(), "+√(9)");
    e.selectAll();
    EXPECT_EQ(e.selectedText(), "2+√(9)");
}

TEST(Entry, ShiftHomeAndEndSelectToTheEdgesOfTheInput) {
    Entry e = fraction({"1"}, {"3"});
    e.extendHome();
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.selectedText(), "((1)/(3))");
    EXPECT_EQ(e.cursor(), 0);
}

TEST(Entry, ASelectionBetweenTwoBoxesCoversTheirTemplate) {
    Entry e = fraction({"1"}, {"3"});
    e.select(Position{{{0, 0}}, 0}, Position{{{0, 1}}, 1});  // from the numerator's start to the denominator's end
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.selectedText(), "((1)/(3))");
    Entry f;
    type(f, {"1", "2", "3"});
    f.select(Position{{}, 3}, Position{{}, 1});  // backwards: the cursor goes where the drag ended
    EXPECT_EQ(f.selectedText(), "23");
    EXPECT_EQ(f.cursor(), 1);
    f.select(Position{{}, 2}, Position{{}, 2});
    EXPECT_FALSE(f.hasSelection());
}

TEST(Entry, ShiftRightAndEndMirrorLeftAndHome) {
    Entry e;
    type(e, {"2", "+"});
    e.insertTemplate(Template::Sqrt);
    type(e, {"9"});
    e.left();         // the start of the radicand
    e.extendRight();  // the 9
    EXPECT_EQ(e.selectedText(), "9");
    e.extendRight();  // past the box: the whole √
    EXPECT_EQ(e.path(), Path{});
    EXPECT_EQ(e.selectedText(), "√(9)");
    EXPECT_EQ(e.cursor(), 3);
    Entry f;
    type(f, {"1", "+"});
    f.insertTemplate(Template::Fraction);
    type(f, {"2"});
    f.extendEnd();  // from inside the numerator: the fraction and everything after it
    EXPECT_EQ(f.path(), Path{});
    EXPECT_EQ(f.selectedText(), "((2)/())");
    EXPECT_EQ(f.cursor(), 3);
    f.home();
    f.extendEnd();
    EXPECT_EQ(f.selectedText(), "1+((2)/())");
}

TEST(Entry, TypingReplacesTheSelection) {
    Entry e;
    type(e, {"1", "+", "2"});
    e.selectAll();
    e.insert("7");
    EXPECT_EQ(e.text(), "7");
    e.selectAll();
    e.backspace();
    EXPECT_TRUE(e.isEmpty());
    type(e, {"4", "5"});
    e.extendLeft();
    e.deleteForward();
    EXPECT_EQ(e.text(), "4");
}

TEST(Entry, FunctionsAndTemplatesWrapTheSelection) {
    const auto selected = [] {
        Entry e;
        type(e, {"1", "+", "2"});
        e.selectAll();
        return e;
    };
    Entry e = selected();
    e.insert("sin(");
    EXPECT_EQ(e.text(), "sin(1+2)");
    EXPECT_FALSE(e.hasSelection());
    EXPECT_EQ(e.cursor(), 5);  // after the closing parenthesis
    e.selectAll();
    e.insert("(");
    EXPECT_EQ(e.text(), "(sin(1+2))");
    Entry f = selected();
    f.insertTemplate(Template::Sqrt);
    EXPECT_EQ(f.text(), "√(1+2)");
    EXPECT_EQ(f.path(), Path{});  // after the root
    Entry g = selected();
    g.insertTemplate(Template::Fraction);
    EXPECT_EQ(g.text(), "((1+2)/())");
    EXPECT_EQ(g.path(), (Path{{0, 1}}));  // on to the denominator
    Entry h = selected();
    h.insertTemplate(Template::Power);
    EXPECT_EQ(h.text(), "(1+2)^()");
    Entry r = selected();
    r.insertTemplate(Template::Root);
    EXPECT_EQ(r.text(), "root(1+2, )");
    EXPECT_EQ(r.path(), (Path{{0, 0}}));  // the index is still to type
}

TEST(Entry, AnEmptyBoxIsUnfinished) {
    Entry e = fraction({"1"}, {});
    EXPECT_TRUE(e.hasEmptyBox());
    type(e, {"2"});
    EXPECT_FALSE(e.hasEmptyBox());
}

TEST(Entry, AnEmptyBoxIsFoundAtAnyDepth) {
    Entry e = fraction({"1"}, {});
    e.insertTemplate(Template::Sqrt);  // in the denominator, which is no longer empty
    EXPECT_TRUE(e.hasEmptyBox());
    type(e, {"4"});
    EXPECT_FALSE(e.hasEmptyBox());
}

TEST(Entry, AByteOfTheTextLeadsBackToItsItem) {
    Entry e;
    e.insert("1");
    e.insert("+");
    e.insertTemplate(Template::Sqrt);
    e.insert("4");
    e.right();
    e.insert("÷");
    e.insert("0");
    ASSERT_EQ(e.text(), "1+√(4)÷0");  // bytes: 1 | + | √ √ √ | ( | 4 | ) | ÷ ÷ | 0
    EXPECT_TRUE(e.positionAt(0) == (Position{{}, 0}));
    EXPECT_TRUE(e.positionAt(2) == (Position{{}, 2}));       // the sign belongs to the template
    EXPECT_TRUE(e.positionAt(6) == (Position{{{2, 0}}, 0}));  // the 4 inside it
    EXPECT_TRUE(e.positionAt(10) == (Position{{}, 4}));
}

TEST(Entry, AByteOfARootLeadsToTheBoxItIsWrittenIn) {
    Entry e;
    e.insertTemplate(Template::Root);
    e.insert("3");  // the index, the first box on the screen
    e.right();
    e.insert("8");
    ASSERT_EQ(e.text(), "root(8, 3)");  // the engine reads the radicand first
    EXPECT_TRUE(e.positionAt(5) == (Position{{{0, 1}}, 0}));
    EXPECT_TRUE(e.positionAt(6) == (Position{{}, 0}));
    EXPECT_TRUE(e.positionAt(8) == (Position{{{0, 0}}, 0}));
    EXPECT_TRUE(e.positionAt(10) == (Position{{}, 1}));  // past the end
}
