#include "typing.hpp"

#include "printers.hpp"

#include <calculate-core/calculate-core.hpp>

#include <QStringList>

#include <algorithm>

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

namespace {

QList<Template> kinds(const Entry& e) {
    QList<Template> list;
    for (const Item& item : e.root()) list << item.kind;
    return list;
}

}  // namespace

TEST(Typing, TheEnginesSpellingsOfTemplatesBecomeTemplates) {
    EXPECT_EQ(kinds(typed("((1)/(3))")), QList<Template>({Template::Fraction}));
    EXPECT_EQ(kinds(typed("root(32, 5)")), QList<Template>({Template::Root}));
    EXPECT_EQ(typed("root(32, 5)").text(), "root(32, 5)");
    EXPECT_EQ(kinds(typed("log(8,2)")), QList<Template>({Template::LogBase}));
    EXPECT_EQ(typed("log(8,2)").text(), "log(8, 2)");
    EXPECT_EQ(kinds(typed("(10^(3))")), QList<Template>({Template::Pow10}));
    EXPECT_EQ(typed("log(8)").text(), "log(8)");     // one argument: log₁₀, a plain piece
    EXPECT_EQ(kinds(typed("(1)/(3)")).size(), 7);    // no outer pair: a plain division
    EXPECT_EQ(typed("1+((1)/(3))").root().size(), 3u);  // the cursor's place adjusts
}

TEST(Typing, SequencesForLaterSyntax) {
    EXPECT_EQ(typed("1->x").text(), "1→x");
    EXPECT_EQ(pieces(typed("1+2 #sqrt(x").root()), QStringList({"1", "+", "2", " ", "#", "s", "q", "r", "t", "(", "x"}));
}

TEST(Typing, ReadSplitsTextIntoTheKeysPieces) {
    const Row row = typing::read("sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(pieces(row), QStringList({"sin(", "1", "e", "1", "0", ")", "+", "Ans", "×", "nCr(", "5", ", ", "2", ")"}));
    Entry e;
    e.setRoot(row);
    EXPECT_EQ(e.text(), "sin(1e10)+Ans×nCr(5, 2)");
    EXPECT_EQ(e.cursor(), 14);
}

TEST(Typing, ReadFinishesEveryBox) {
    const Row row = typing::read("sqrt(2");  // the template closes what the text left open
    ASSERT_EQ(row.size(), 1u);
    EXPECT_EQ(row[0].closing, Closing::Key);
    EXPECT_EQ(typing::read("2^10")[1].closing, Closing::Key);
}

// Every template as its key makes it, holding a second template in two-box cases: the text it gives reads
// back into exactly the same items, so "copy expression" and paste undo each other.
TEST(Typing, CopiedTextReadsBackIntoTheSameTemplates) {
    for (Template t : {Template::Fraction, Template::Sqrt, Template::Cbrt, Template::Root, Template::Power, Template::Exp,
                       Template::Pow10, Template::LogBase, Template::Abs}) {
        Entry e;
        e.insert("1");
        e.insert("+");
        e.insertTemplate(t);
        e.insert("7");
        if (t == Template::Fraction || t == Template::Root || t == Template::LogBase) {
            e.right();
            e.insertTemplate(Template::Sqrt);
            e.insert("9");
            e.right();
        }
        e.right();
        e.insert("−");
        e.insert("4");
        EXPECT_TRUE(typing::read(e.text()) == e.root()) << e.text().toStdString();
    }
}

namespace {

QString valueOf(const QString& expression) {
    const calculate_core::Result r = calculate_core::evaluate(expression.toStdString());
    if (r.error) return QStringLiteral("error ") + QString::number(static_cast<int>(r.error->code));
    return QString::fromStdString(r.value.digits) + "e" + QString::number(r.value.exponent10);
}

}  // namespace

// Read into templates and written back, linear text keeps its value.
TEST(Typing, PastedTextMeansWhatItSays) {
    for (const char* text : {"2^10+1", "-2^2", "2^-1", "√4+5", "√2^2", "sqrt(16)/4", "1/3", "((1)/(3))+1", "root(27, 3)",
                             "log(8, 2)", "log(100)", "10^3", "(10^(3))", "e^1", "exp(1)", "abs(-3)*2", "3!^2", "2^3!",
                             "nCr(5, 2)", "50%", "1e3+2", "1.5e-3*2", "pi*2", "mean(1, 2, 3)", "2^(1+2)*3"}) {
        Entry e;
        e.setRoot(typing::read(QString::fromUtf8(text)));
        EXPECT_EQ(valueOf(e.text()), valueOf(QString::fromUtf8(text))) << text << " → " << e.text().toStdString();
    }
    Entry e;
    e.setRoot(typing::read("2**3**2"));  // the engine doesn't read ** yet: compared with what it means
    EXPECT_EQ(valueOf(e.text()), valueOf("2^3^2"));
}

TEST(Typing, PasteGoesInAtTheCursor) {
    Entry e = typed("1+");
    typing::paste(e, "sqrt(4)*2");
    EXPECT_EQ(e.text(), "1+√(4)×2");
    EXPECT_EQ(e.path(), (std::vector<std::pair<int, int>>{}));
    EXPECT_EQ(e.cursor(), 5);  // 1 + √(4) × 2
}

TEST(Typing, TypingOverASelectionReplacesOrWrapsIt) {
    Entry e = typed("pi");
    e.selectAll();
    typing::typeCharacter(e, '+');  // replaced, not first finished into π
    EXPECT_EQ(e.text(), "+");
    Entry f = typed("1+2");
    f.selectAll();
    typing::typeCharacter(f, '(');
    EXPECT_EQ(f.text(), "(1+2)");
    Entry g = typed("sin");
    g.selectAll();
    typing::typeCharacter(g, '(');  // wraps: no name lookup
    EXPECT_EQ(g.text(), "(sin)");
    Entry h = typed("2^10");
    h.extendLeft();
    h.extendLeft();  // the 10, inside the exponent
    typing::typeCharacter(h, '^');  // the selection is the new power's base, where it is
    EXPECT_EQ(h.text(), "2^((10)^())");
    Entry r = typed("√12");
    r.left();
    r.left();
    r.extendRight();
    r.extendRight();  // the 12, the cursor at the end of the root's box, where a ^ would end it
    typing::typeCharacter(r, '^');
    EXPECT_EQ(r.text(), "√((12)^())");
    Entry k = typed("12");
    k.selectAll();
    typing::typeCharacter(k, QChar(0x221A));  // √ wraps it and is done
    EXPECT_EQ(k.text(), "√(12)");
    EXPECT_EQ(k.root()[0].closing, Closing::Key);
}

TEST(Typing, CompletionsListTheNamesThatStartWithWhatIsTyped) {
    EXPECT_EQ(typing::completions("sq"), QStringList({"sqrt"}));
    const QStringList as = typing::completions("as");
    EXPECT_TRUE(as.contains("asin"));
    EXPECT_TRUE(as.contains("asinh"));
    EXPECT_FALSE(as.contains("sin"));
    EXPECT_TRUE(typing::completions("arcs").contains("arcsen"));  // the Spanish names too
    EXPECT_TRUE(typing::completions("A").contains("Ans"));
    EXPECT_TRUE(typing::completions("").isEmpty());
    EXPECT_EQ(typing::nameBeingTyped(typed("2+co")), "co");
    EXPECT_EQ(typing::nameBeingTyped(typed("2+")), "");
}

TEST(Typing, CompletingANameTypesItsParenthesis) {
    Entry e = typed("2+sq");
    typing::complete(e, "sqrt");
    ASSERT_EQ(e.root().size(), 3u);
    EXPECT_EQ(e.root()[2].kind, Template::Sqrt);  // the cursor is in its box
    Entry p = typed("p");
    typing::complete(p, "pi");
    EXPECT_EQ(p.text(), "π");
    Entry n = typed("nc");
    typing::complete(n, "nCr");
    EXPECT_EQ(n.text(), "nCr(");
}

TEST(Typing, CompletionsAreSortedAndCaseSensitive) {
    EXPECT_FALSE(typing::completions("A").contains("asin"));  // as the engine: Asin is not asin
    QStringList sorted = typing::completions("a");
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(typing::completions("a"), sorted);
    EXPECT_GT(sorted.size(), 5);
}
