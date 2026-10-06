#include "typing.hpp"

#include "printers.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QStringList>

#include <gtest/gtest.h>

namespace {

// What typing `keys` one character at a time gives.
Entry typed(const QString& keys) {
    Entry e;
    for (const QChar c : keys) typing::typeCharacter(e, c);
    return e;
}

QStringList pieces(const Row& row) {
    QStringList list;
    for (const Item& item : row) list << (item.kind == Template::Text ? item.text : QStringLiteral("<template>"));
    return list;
}

}  // namespace

TEST(Typing, OperatorsBecomeTheCalculatorsSigns) {
    EXPECT_EQ(typed("12+3*4/5-6").text(), "12+3×4÷5−6");
    EXPECT_EQ(typed("(1.5)").text(), "(1.5)");
    EXPECT_EQ(typed("3!+50%").text(), "3!+50%");
}

TEST(Typing, ANameBeforeAParenthesisIsOnePiece) {
    const Entry e = typed("sin(30)");
    EXPECT_EQ(pieces(e.root()), QStringList({"sin(", "3", "0", ")"}));
    EXPECT_EQ(e.text(), "sin(30)");
    EXPECT_EQ(pieces(typed("sen(").root()), QStringList({"sen("}));  // any name: the engine judges it
    EXPECT_EQ(pieces(typed("1e10(").root()), QStringList({"1", "e", "1", "0", "("}));  // an exponent, not a name
}

TEST(Typing, ConstantsAndRegistersBecomeTheirKeysPieces) {
    EXPECT_EQ(pieces(typed("pi+Ans*M-e").root()), QStringList({"π", "+", "Ans", "×", "M", "−", "e"}));
    Entry e = typed("pi");  // still being typed: letters until the name ends
    EXPECT_EQ(pieces(e.root()), QStringList({"p", "i"}));
    typing::finishName(e);
    EXPECT_EQ(pieces(e.root()), QStringList({"π"}));
    EXPECT_EQ(pieces(typed("pin+").root()), QStringList({"p", "i", "n", "+"}));  // not a name we know: letters
}

TEST(Typing, SpacesAreKeptOnce) {
    EXPECT_EQ(typed("1 + 2").text(), "1 + 2");
    EXPECT_EQ(typed("1  +2").text(), "1 +2");
    EXPECT_EQ(typed("1 000").text(), "1000");  // a thin space groups digits: ignored
    EXPECT_FALSE(typing::typeCharacter(*new Entry, QChar('\n')));
}

TEST(Typing, TheCommaSeparatesArgumentsOnlyInACallThatTakesSeveral) {
    EXPECT_EQ(typed("3,5").text(), "3.5");  // a decimal point, as before
    EXPECT_EQ(typed("nCr(5,2)").text(), "nCr(5, 2)");
    EXPECT_EQ(typed("sin(0,5)").text(), "sin(0.5)");      // sin takes one argument
    EXPECT_EQ(typed("mcd(12,18)").text(), "mcd(12, 18)");  // the Spanish names too
    EXPECT_EQ(typed("nCr(5,(1,5))").text(), "nCr(5, (1.5))");  // inner parentheses are not the call
    EXPECT_EQ(typed("nCr(5, 2)").text(), "nCr(5, 2)");     // a space after the separator adds nothing
    EXPECT_EQ(typed("sin(1;2)").text(), "sin(1, 2)");      // ; always separates (the engine then says sin takes 1)
    EXPECT_EQ(typed("3;5").text(), "3, 5");
}

TEST(Typing, TypedRootsAndAbsAreTheKeysTemplates) {
    Entry e = typed("sqrt(2");
    ASSERT_EQ(e.root().size(), 1u);
    EXPECT_EQ(e.root()[0].kind, Template::Sqrt);
    EXPECT_EQ(e.path(), (std::vector<std::pair<int, int>>{{0, 0}}));  // typing goes on inside
    for (const QChar c : QString(")+1")) typing::typeCharacter(e, c);
    EXPECT_EQ(e.text(), "√(2)+1");                  // ) left the root
    EXPECT_EQ(e.root()[0].closing, Closing::Key);  // finished: from now on like a key's
    EXPECT_EQ(typed("cbrt(8)").root()[0].kind, Template::Cbrt);
    EXPECT_EQ(typed("abs(-3)").text(), "abs(−3)");
    EXPECT_EQ(typed("exp(1)").root()[0].kind, Template::Exp);
    EXPECT_EQ(typed("sqrt((1+2)*3)").text(), "√((1+2)×3)");  // the ) of an inner pair stays a piece
}

TEST(Typing, ACaretOpensAnExponentThatEndsLikeLinearText) {
    EXPECT_EQ(typed("2^10+1").text(), "2^(10)+1");  // 1025, as the text means
    EXPECT_EQ(typed("2^-3*4").text(), "2^(−3)×4");
    EXPECT_EQ(typed("2^3^2").text(), "2^(3^(2))");  // a tower, right to left
    EXPECT_EQ(typed("2^(1+2)*3").text(), "2^(1+2)×3");  // its own parentheses are the box's
    EXPECT_EQ(typed("2^3!").text(), "2^(3!)");
    EXPECT_EQ(typed("2^1e-5").text(), "2^(1e−5)");  // a number's exponent sign stays in the number
    EXPECT_EQ(typed("2**3").text(), "2^(3)");
    EXPECT_EQ(typed("x²+1").text(), "x^(2)+1");
    EXPECT_EQ(typed("nCr(2^3,4)").text(), "nCr(2^(3), 4)");  // the separator ends the exponent too
    EXPECT_EQ(typed("2^10 to").text(), "2^(10) to");      // a word after a space is not the exponent
}

TEST(Typing, TheRootSignTakesTheNextOperand) {
    EXPECT_EQ(typed("√4+5").text(), "√(4)+5");
    EXPECT_EQ(typed("√2^2").text(), "√(2)^(2)");  // √ binds tighter than ^, as in linear text
    EXPECT_EQ(typed("√(4+5)").text(), "√(4+5)");
    EXPECT_EQ(typed("∛8").root()[0].kind, Template::Cbrt);
}
