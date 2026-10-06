#include "entry.hpp"
#include "typeset.hpp"
#include "typing.hpp"

#include "printers.hpp"

#include <QFontDatabase>
#include <QFontMetricsF>

#include <gtest/gtest.h>

#include <algorithm>

using typeset::Box;
using typeset::Role;

namespace {

QFont font() {
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPixelSize(20);
    return f;
}

qreal width(const typeset::Run& run) { return QFontMetricsF(run.font).horizontalAdvance(run.text); }
qreal top(const typeset::Run& run) { return run.origin.y() - QFontMetricsF(run.font).ascent(); }
qreal bottom(const typeset::Run& run) { return run.origin.y() + QFontMetricsF(run.font).descent(); }
qreal centre(const typeset::Run& run) { return run.origin.x() + width(run) / 2; }

}  // namespace

TEST(Typeset, AFractionStacksTheNumeratorOverTheDenominator) {
    const Box b = typeset::fraction(typeset::text("1", font()), typeset::text("12", font()), font());
    ASSERT_EQ(b.runs.size(), 2);
    ASSERT_EQ(b.lines.size(), 1);
    const typeset::Run& numerator = b.runs[0];
    const typeset::Run& denominator = b.runs[1];
    const QLineF bar = b.lines[0];
    EXPECT_LT(bottom(numerator), bar.y1());
    EXPECT_GT(top(denominator), bar.y1());
    EXPECT_LT(bar.y1(), 0);  // the bar sits above the baseline, at the height of a minus sign
    EXPECT_NEAR(centre(numerator), (bar.x1() + bar.x2()) / 2, 0.01);
    EXPECT_NEAR(centre(denominator), (bar.x1() + bar.x2()) / 2, 0.01);
    EXPECT_GE(bar.length(), width(denominator));
    EXPECT_NEAR(b.width, bar.x2(), 0.01);
    EXPECT_NEAR(-b.ascent, top(numerator), 0.01);
    EXPECT_NEAR(b.descent, bottom(denominator), 0.01);
}

TEST(Typeset, AnExponentIsRaisedAndSmaller) {
    QFont small = font();
    small.setPixelSize(14);
    const Box b = typeset::superscript(typeset::text("×10", font()), typeset::text("−7", small));
    ASSERT_EQ(b.runs.size(), 2);
    EXPECT_LT(b.runs[1].origin.y(), b.runs[0].origin.y());
    EXPECT_NEAR(b.runs[1].origin.x(), width(b.runs[0]), 0.01);
    EXPECT_LT(b.runs[1].font.pixelSize(), b.runs[0].font.pixelSize());
}

TEST(Typeset, AnOverlineSpansItsDigitsOnly) {
    const Box b = typeset::row({typeset::text("0.1", font()), typeset::overline(typeset::text("6", font()))});
    ASSERT_EQ(b.runs.size(), 2);
    ASSERT_EQ(b.lines.size(), 1);
    const typeset::Run& six = b.runs[1];
    EXPECT_NEAR(b.lines[0].x1(), six.origin.x(), 0.01);
    EXPECT_NEAR(b.lines[0].x2(), six.origin.x() + width(six), 0.01);
    EXPECT_LT(b.lines[0].y1(), top(six));
}

TEST(Typeset, LongTextWrapsAtTheWidthAndKeepsItsRoles) {
    const qreal digit = QFontMetricsF(font()).horizontalAdvance('0');
    const Box b = typeset::paragraph({{QString(25, '1'), Role::Plain}, {QString(15, '2'), Role::Noise}}, font(), 10 * digit);
    // 40 characters, 10 per line: 4 lines; the third holds 5 plain and 5 noise digits.
    QList<qreal> baselines;
    for (const typeset::Run& run : b.runs) {
        EXPECT_LE(run.origin.x() + width(run), 10 * digit + 0.01);
        if (!baselines.contains(run.origin.y())) baselines << run.origin.y();
    }
    EXPECT_EQ(baselines.size(), 4);
    ASSERT_EQ(b.runs.size(), 5);
    EXPECT_EQ(b.runs[2].text, "11111");
    EXPECT_EQ(b.runs[2].role, Role::Plain);
    EXPECT_EQ(b.runs[3].text, "22222");
    EXPECT_EQ(b.runs[3].role, Role::Noise);
    EXPECT_EQ(b.runs[2].origin.y(), b.runs[3].origin.y());
}

TEST(Typeset, AValueShowsItsNoiseAfterABarAndItsExponentRaised) {
    const Box b = typeset::value({"−1.000000000000000", "019884624838656", "30"}, font(), 1000);
    ASSERT_EQ(b.runs.size(), 4);
    EXPECT_EQ(b.runs[0].text, "−1.000000000000000");
    EXPECT_EQ(b.runs[1].text, "|019884624838656");
    EXPECT_EQ(b.runs[1].role, Role::Noise);
    EXPECT_EQ(b.runs[2].text, "×10");
    EXPECT_EQ(b.runs[3].text, "30");
    EXPECT_LT(b.runs[3].origin.y(), b.runs[2].origin.y());
}

TEST(Typeset, AnExactResultIsTheFractionEqualsItsDecimal) {
    const Box b = typeset::exact({"", "1", "6", "0.1", "6"}, font(), 1000);
    QStringList texts;
    for (const typeset::Run& run : b.runs) texts << run.text;
    EXPECT_EQ(texts, QStringList({"1", "6", " = ", "0.1", "6"}));
    EXPECT_EQ(b.lines.size(), 2);  // the fraction bar and the recurring line
    const Box whole = typeset::exact({"−", "6", "1", "", ""}, font(), 1000);
    ASSERT_EQ(whole.runs.size(), 1);
    EXPECT_EQ(whole.runs[0].text, "−6");
}

namespace {

QFont smallFont() {
    QFont f = font();
    f.setPixelSize(14);
    return f;
}

}  // namespace

TEST(Typeset, ASubscriptIsLoweredAndSmaller) {
    const Box b = typeset::subscript(typeset::text("x", font()), typeset::text("i", smallFont()));
    ASSERT_EQ(b.runs.size(), 2);
    EXPECT_GT(b.runs[1].origin.y(), b.runs[0].origin.y());
    EXPECT_NEAR(b.runs[1].origin.x(), width(b.runs[0]), 0.01);
    EXPECT_GT(b.descent, QFontMetricsF(font()).descent());
}

TEST(Typeset, ARadicalsBarSpansItsContent) {
    const Box b = typeset::radical(typeset::text("12", font()), font());
    ASSERT_EQ(b.runs.size(), 1);
    const typeset::Run& content = b.runs[0];
    EXPECT_GT(content.origin.x(), 0);  // after the radical's tick
    const auto bar = std::find_if(b.lines.begin(), b.lines.end(), [](const QLineF& l) { return l.y1() == l.y2(); });
    ASSERT_NE(bar, b.lines.end());
    EXPECT_LT(bar->y1(), top(content));
    EXPECT_LE(bar->x1(), content.origin.x());
    EXPECT_GE(bar->x2(), content.origin.x() + width(content));
    EXPECT_GT(b.ascent, QFontMetricsF(font()).ascent());
}

TEST(Typeset, ARootsIndexSitsOverTheTick) {
    const Box b = typeset::radical(typeset::text("32", font()), font(), typeset::text("5", smallFont()));
    ASSERT_EQ(b.runs.size(), 2);
    const typeset::Run& index = b.runs[0];
    const typeset::Run& content = b.runs[1];
    EXPECT_LT(index.origin.x() + width(index), content.origin.x());
    EXPECT_LT(index.origin.y(), content.origin.y());
}

TEST(Typeset, ABigOperatorCentresItsLimits) {
    const Box b = typeset::bigOperator("Σ", typeset::text("i=1", smallFont()), typeset::text("n", smallFont()), font());
    ASSERT_EQ(b.runs.size(), 3);
    const typeset::Run& symbol = b.runs[0];
    const typeset::Run& under = b.runs[1];
    const typeset::Run& over = b.runs[2];
    EXPECT_NEAR(centre(under), centre(symbol), 0.6);
    EXPECT_NEAR(centre(over), centre(symbol), 0.6);
    // the limits hug the glyph itself (its ink, not the font's line height)
    const QRectF ink = QFontMetricsF(symbol.font).tightBoundingRect(symbol.text).translated(symbol.origin);
    const qreal close = QFontMetricsF(font()).height() * 0.3;
    EXPECT_GE(top(under), ink.bottom());
    EXPECT_LT(top(under) - ink.bottom(), close);
    EXPECT_LE(bottom(over), ink.top());
    EXPECT_LT(ink.top() - bottom(over), close);
    EXPECT_GT(symbol.font.pixelSize(), font().pixelSize());  // a big Σ
}

namespace {

const typeset::Run* runWith(const Box& b, const QString& text) {
    for (const typeset::Run& run : b.runs)
        if (run.text == text) return &run;
    return nullptr;
}

}  // namespace

TEST(Typeset, TheInputDrawsAFractionWithTheCursorInside) {
    Entry e;
    e.insertTemplate(Template::Fraction);
    e.insert("1");
    QRectF caret;
    const Box b = typeset::input(e, font(), &caret);
    const typeset::Run* one = runWith(b, "1");
    const typeset::Run* empty = runWith(b, "□");  // the empty denominator
    ASSERT_NE(one, nullptr);
    ASSERT_NE(empty, nullptr);
    EXPECT_LT(bottom(*one), top(*empty));  // stacked, not side by side
    EXPECT_NEAR(caret.left(), one->origin.x() + width(*one), 0.01);  // right after the 1
    EXPECT_LE(caret.bottom(), top(*empty));                          // in the numerator
    ASSERT_EQ(b.lines.size(), 1);                                    // the fraction bar
}

TEST(Typeset, AnEmptyInputDrawsNothingButTheCursor) {
    QRectF caret;
    const Box b = typeset::input(Entry{}, font(), &caret);
    EXPECT_TRUE(b.runs.isEmpty());  // no placeholder: only template boxes show one
    EXPECT_EQ(b.width, 0);
    EXPECT_EQ(caret.left(), 0);
    EXPECT_GT(caret.height(), 0);  // the cursor still has a line's height
}

TEST(Typeset, PlainInputStaysOnOneLineWithTheCursorAtTheEnd) {
    Entry e;
    e.setRoot(typing::read("12+3"));
    QRectF caret;
    const Box b = typeset::input(e, font(), &caret);
    for (const typeset::Run& run : b.runs) EXPECT_EQ(run.origin.y(), 0);
    EXPECT_NEAR(caret.left(), b.width, 0.01);
    EXPECT_TRUE(b.lines.isEmpty());
}

TEST(Typeset, TheInputRaisesExponentsAndDrawsRadicals) {
    Entry e;
    e.insert("2");
    e.insertTemplate(Template::Power);
    e.insert("8");
    e.right();
    e.insertTemplate(Template::Sqrt);
    e.insert("9");
    QRectF caret;
    const Box b = typeset::input(e, font(), &caret);
    EXPECT_LT(runWith(b, "8")->origin.y(), runWith(b, "2")->origin.y());  // the exponent is raised
    EXPECT_LT(runWith(b, "8")->font.pixelSize(), runWith(b, "2")->font.pixelSize());
    EXPECT_GE(b.lines.size(), 4);  // the radical's strokes
}

TEST(Typeset, EveryPlaceForTheCursorIsMarked) {
    Entry e;
    e.setRoot(typing::read("12+3"));
    const Box b = typeset::input(e, font(), nullptr);
    ASSERT_EQ(b.marks.size(), 5);  // before each of the four pieces, and at the end
    for (int i = 1; i < b.marks.size(); ++i) EXPECT_GT(b.marks[i].caret.left(), b.marks[i - 1].caret.left());
    EXPECT_TRUE(typeset::hit(b, QPointF(b.marks[2].caret.left() + 1, 0)) == (Position{{}, 2}));
    EXPECT_TRUE(typeset::hit(b, QPointF(-50, 0)) == (Position{{}, 0}));
    EXPECT_TRUE(typeset::hit(b, QPointF(b.width + 50, 0)) == (Position{{}, 4}));
}

TEST(Typeset, AClickInADenominatorPutsTheCursorThere) {
    Entry e;
    e.insertTemplate(Template::Fraction);
    e.insert("1");
    e.right();
    e.insert("3");
    e.right();
    const Box b = typeset::input(e, font(), nullptr);
    const typeset::Run* three = runWith(b, "3");
    ASSERT_NE(three, nullptr);
    const Position p = typeset::hit(b, QPointF(three->origin.x() + width(*three) + 1, three->origin.y() - 2));
    EXPECT_TRUE(p == (Position{{{0, 1}}, 1}));  // after the 3, in the denominator
}

TEST(Typeset, TheSelectionCoversWholeTemplates) {
    Entry e;
    e.insert("2");
    e.insertTemplate(Template::Fraction);
    e.insert("1");
    e.right();
    e.insert("3");
    e.right();
    e.selectAll();
    const Box b = typeset::input(e, font(), nullptr);
    const QRectF s = typeset::selectionRect(b, e);
    EXPECT_LE(s.top(), top(*runWith(b, "1")));
    EXPECT_GE(s.bottom(), bottom(*runWith(b, "3")));
    EXPECT_NEAR(s.left(), 0, 0.01);
    EXPECT_NEAR(s.width(), b.width, 0.01);
    e.end();  // no selection, no rectangle
    EXPECT_TRUE(typeset::selectionRect(typeset::input(e, font(), nullptr), e).isEmpty());
}
