#include "presenter.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

#include <QStringList>

using namespace calculate_core;

namespace {

Result evaluated(const char* text, NumberType type = NumberType::Double) {
    Options o;
    o.type = type;
    return evaluate(text, o);
}

TypeInfo typeInfo(NumberType type) { return numberTypes()[static_cast<std::size_t>(type)]; }

}  // namespace

TEST(Presenter, TypeLabelsShowTheCppNameAndTheTraits) {
    EXPECT_EQ(view::typeLabel(typeInfo(NumberType::Double)), "Double · double · 64-bit · ~16 digits");
    EXPECT_EQ(view::typeLabel(typeInfo(NumberType::Exact)), "Exact · cpp_rational · no rounding");
    EXPECT_EQ(view::typeLabel(typeInfo(NumberType::Binary256)),
              "Octuple · binary256 · 256-bit · ~71 digits · software, no subnormals");
}

TEST(Presenter, ConditionVerdicts) {
    EXPECT_EQ(view::verdict("1e+0"), "well-conditioned");
    EXPECT_EQ(view::verdict("1.8e+5"), "moderately conditioned");
    EXPECT_EQ(view::verdict("2e+16"), "ill-conditioned: no algorithm can do better in this type");
    EXPECT_EQ(view::verdict("inf"), "ill-conditioned: no algorithm can do better in this type");
    EXPECT_EQ(view::verdict("5e-1"), "well-conditioned");
}

TEST(Presenter, TheDetailsCardHoldsEveryFigure) {
    const QList<view::DetailRow> rows = view::details(evaluated("0.1 + 0.2"), typeInfo(NumberType::Double));
    QStringList keys;
    for (const view::DetailRow& row : rows) keys << row.key;
    EXPECT_EQ(keys, QStringList({"bound", "measured", "trusted", "condition", "input", "rounding", "library",
                                 "operations", "type", "evaluated"}));
    EXPECT_EQ(rows[0].label, "Guaranteed bound");
    EXPECT_EQ(rows[0].value, "4.4e-17");
    EXPECT_EQ(rows[1].value, "4.4e-17");
    EXPECT_EQ(rows[2].value, "15 by the bound, 15 by the measurement");
    EXPECT_EQ(rows[3].value, "1e+0 · well-conditioned");
    EXPECT_EQ(rows[4].value, "1.7e-17");
    EXPECT_EQ(rows[5].value, "2.8e-17");
    EXPECT_EQ(rows[6].value, "0");
    EXPECT_EQ(rows[7].value, "1");
    EXPECT_EQ(rows[8].value, "double, 53-bit significand");
    EXPECT_EQ(rows[9].value, "0.1 + 0.2");
}

TEST(Presenter, ExactAndIncompleteResultsSaySo) {
    const QList<view::DetailRow> exact = view::details(evaluated("1/3", NumberType::Exact), typeInfo(NumberType::Exact));
    ASSERT_EQ(exact.size(), 3);
    EXPECT_EQ(exact[0].key, "exact");
    EXPECT_EQ(exact[0].value, "exact · no rounding error");
    EXPECT_EQ(exact[1].value, "cpp_rational, exact fractions");
    Options allow;
    allow.allowUncertainDiscreteArguments = true;
    const QList<view::DetailRow> rows = view::details(evaluate("(0.1*30)!", allow), typeInfo(NumberType::Double));
    ASSERT_GE(rows.size(), 2);
    EXPECT_EQ(rows[rows.size() - 2].key, "incomplete");
    EXPECT_EQ(rows[rows.size() - 2].value, "an uncertain argument was accepted");
    EXPECT_TRUE(view::details(evaluated("1/0"), typeInfo(NumberType::Double)).isEmpty());
}

TEST(Presenter, EveryDetailIsExplained) {
    for (const char* key : {"bound", "measured", "trusted", "condition", "input", "rounding", "library", "operations",
                            "type", "evaluated", "exact", "incomplete"})
        EXPECT_FALSE(view::explanation(key).isEmpty()) << key;
    EXPECT_TRUE(view::explanation("nonsense").isEmpty());
}

TEST(Presenter, ErrorsNameWhatWentWrong) {
    const auto text = [](const char* expression, NumberType type = NumberType::Double) {
        const Result r = evaluated(expression, type);
        EXPECT_TRUE(r.error) << expression;
        return view::errorText(*r.error, QString::fromUtf8(expression));
    };
    EXPECT_EQ(text("1 + 1/0"), "Division by zero");
    EXPECT_EQ(text("2π"), "Missing operator before “π” (write 2×π, not 2π)");
    EXPECT_EQ(text("sin(1)", NumberType::Exact),
              "Exact arithmetic cannot represent sin: its result is irrational. Switch to a floating type to compute it.");
    EXPECT_EQ(text("sqrt(2)", NumberType::Exact), "The exact result of sqrt(2) is irrational");
    EXPECT_EQ(text("Ans"), "There is no previous result yet");
    EXPECT_EQ(text("foo(1)"), "Unknown name “foo”");
    EXPECT_EQ(text("(0.1*30)!"),
              "(0.1*30)! needs an exactly known whole number, but its argument carries an error. "
              "If you proceed anyway, the error report will not include that error.");
}

TEST(Presenter, StatisticsExpressions) {
    EXPECT_EQ(view::statisticsExpression("mean", "1\n2, 3;  4\n\n"), "mean(1, 2, 3, 4)");
    EXPECT_EQ(view::statisticsExpression("stdev", "  \n "), "");
}

TEST(Presenter, ValuePartsSeparateTrustedDigitsNoiseAndExponent) {
    view::ValueParts p = view::valueParts(evaluated("0.1 + 0.2"));
    EXPECT_EQ(p.trusted, "0.300000000000000");
    EXPECT_EQ(p.noise, "0444089209850062616169452667236328125");
    EXPECT_EQ(p.exponent, "");
    p = view::valueParts(evaluated("-1e30"));
    EXPECT_EQ(p.trusted, "−1.000000000000000");
    EXPECT_EQ(p.noise, "019884624838656");
    EXPECT_EQ(p.exponent, "30");
    p = view::valueParts(evaluated("1e-30"));
    EXPECT_EQ(p.trusted, "1.000000000000000");
    EXPECT_EQ(p.exponent, "−30");
    p = view::valueParts(evaluated("2+2"));
    EXPECT_EQ(p.trusted, "4");
    EXPECT_EQ(p.noise, "");
}

TEST(Presenter, FractionPartsStackTheFractionOverItsDecimal) {
    view::FractionParts f = view::fractionParts(evaluated("1/3", NumberType::Exact));
    EXPECT_EQ(f.sign, "");
    EXPECT_EQ(f.numerator, "1");
    EXPECT_EQ(f.denominator, "3");
    EXPECT_EQ(f.decimal, "0.");
    EXPECT_EQ(f.recurring, "3");
    f = view::fractionParts(evaluated("-7/4", NumberType::Exact));
    EXPECT_EQ(f.sign, "−");
    EXPECT_EQ(f.numerator, "7");
    EXPECT_EQ(f.decimal, "1.75");
    EXPECT_EQ(f.recurring, "");
    f = view::fractionParts(evaluated("1/6", NumberType::Exact));
    EXPECT_EQ(f.decimal, "0.1");
    EXPECT_EQ(f.recurring, "6");
    f = view::fractionParts(evaluated("6", NumberType::Exact));
    EXPECT_EQ(f.denominator, "1");
    EXPECT_EQ(f.decimal, "");  // a whole number needs no decimal
    f = view::fractionParts(evaluated("1/97", NumberType::Exact));
    EXPECT_EQ(f.decimal, "");  // its period is too long to show
}

TEST(Presenter, ShortTypeNamesForTheCompactSelector) {
    QStringList names;
    for (const TypeInfo& t : numberTypes()) names << view::shortTypeName(t);
    EXPECT_EQ(names, QStringList({"Single", "Double", "Extended", "Exact", "Quadruple", "Octuple", "Hexadecuple"}));
    EXPECT_TRUE(view::typeLabel(typeInfo(NumberType::Binary512)).startsWith("Hexadecuple · binary512"));
}

TEST(Presenter, TypeTraitsForTheTypeMenu) {
    EXPECT_EQ(view::typeDigits(typeInfo(NumberType::Double)), "~16 significant digits");
    EXPECT_EQ(view::typeDetail(typeInfo(NumberType::Double)), "double · 64-bit storage · 53-bit significand");
    EXPECT_EQ(view::typeDigits(typeInfo(NumberType::Exact)), "every digit exact");
    EXPECT_EQ(view::typeDetail(typeInfo(NumberType::Exact)), "cpp_rational · fractions, no rounding");
    EXPECT_EQ(view::typeDetail(typeInfo(NumberType::Binary512)),
              "binary512 · 512-bit storage · 489-bit significand · software, no subnormals");
}
