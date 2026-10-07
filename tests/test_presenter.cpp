#include "presenter.hpp"

#include "entry.hpp"
#include "printers.hpp"
#include "settings.hpp"
#include "typing.hpp"

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
    EXPECT_EQ(view::typeLabel(typeInfo(NumberType::Binary256)), "Octuple · binary256 · 256-bit · ~71 digits");
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
    EXPECT_EQ(view::typeDetail(typeInfo(NumberType::Binary512)), "binary512 · 512-bit storage · 489-bit significand");
}

TEST(Presenter, EveryStatisticSaysHowItIsComputed) {
    for (const char* f : {"mean", "median", "var", "stdev", "varp", "stdevp"}) EXPECT_FALSE(view::algorithm(f).isEmpty()) << f;
    EXPECT_TRUE(view::algorithm("variance").isEmpty());
    EXPECT_NE(view::algorithm("var"), view::algorithm("varp"));  // n − 1 against n
}

TEST(Presenter, TheBoundIsExplainedAsProven) {
    EXPECT_EQ(view::explanation("bound"), "A proven upper limit on how far the shown value can be from the exact result.");
}

TEST(Presenter, AJumpWithinTheErrorIsExplained) {
    const Result r = evaluated("mod(0.7 + 0.1, 0.8)");
    ASSERT_TRUE(r.error);
    EXPECT_EQ(view::errorText(*r.error, "mod(0.7 + 0.1, 0.8)"),
              "mod(0.7 + 0.1, 0.8) jumps within the error of its arguments, so the result could be off by a whole step. "
              "If you proceed anyway, the error report will not include that error.");
}

TEST(Presenter, AnEdgeWithinTheErrorIsExplained) {
    const Result r = evaluated("sqrt(0.1+0.2-0.3)");
    ASSERT_TRUE(r.error);
    EXPECT_EQ(view::errorText(*r.error, "sqrt(0.1+0.2-0.3)"),
              "The error of the argument of sqrt(0.1+0.2-0.3) reaches a point where it is not defined or not smooth, "
              "so no bound can be given. If you proceed anyway, the error report will not include that.");
}

TEST(Presenter, UnfinishedExpressionsAreToldApart) {
    EXPECT_TRUE(view::incomplete(*evaluate("2*").error));
    EXPECT_TRUE(view::incomplete(*evaluate("sin(1").error));
    EXPECT_FALSE(view::incomplete(*evaluate("1/0").error));
    EXPECT_FALSE(view::incomplete(*evaluate("2 3").error));
}

namespace {

Result floating(const char* digits, long long exponent, int trusted) {
    Result r;
    r.value = Digits{false, digits, exponent};
    r.trustedDigits = trusted;
    r.bound = "1e0";
    return r;
}

}  // namespace

TEST(Presenter, CopiedValuesArePlainText) {
    const TypeInfo d = typeInfo(NumberType::Double);
    const Result sum = evaluated("0.1 + 0.2");
    EXPECT_EQ(view::copyText(sum, view::CopyForm::Value, d), "0.3000000000000000444089209850062616169452667236328125");
    EXPECT_EQ(view::copyText(sum, view::CopyForm::Trusted, d), "0.300000000000000");
    EXPECT_EQ(view::copyText(sum, view::CopyForm::ValueAndBound, d),
              "0.3000000000000000444089209850062616169452667236328125 ± 4.4e-17");
    const Result big = evaluated("-1e30");
    EXPECT_EQ(view::copyText(big, view::CopyForm::Value, d), "-1.000000000000000019884624838656e30");
    EXPECT_EQ(view::copyText(big, view::CopyForm::Trusted, d), "-1.000000000000000e30");
    EXPECT_TRUE(view::copyText(evaluated("1e-30"), view::CopyForm::Value, d).endsWith("e-30"));
}

TEST(Presenter, NothingTrustedCopiesNothing) {
    const TypeInfo d = typeInfo(NumberType::Double);
    EXPECT_EQ(view::copyText(floating("5", 0, 0), view::CopyForm::Trusted, d), "");
    EXPECT_EQ(view::copyText(floating("5", 0, 0), view::CopyForm::Value, d), "5");
    EXPECT_EQ(view::copyText(floating("25", 0, 1), view::CopyForm::Trusted, d), "2");  // "2." loses its point
}

TEST(Presenter, ExactResultsCopyAsFractions) {
    const TypeInfo x = typeInfo(NumberType::Exact);
    const Result third = evaluated("1/3", NumberType::Exact);
    EXPECT_EQ(view::copyText(third, view::CopyForm::Value, x), "1/3");
    EXPECT_EQ(view::copyText(third, view::CopyForm::Trusted, x), "1/3");
    EXPECT_EQ(view::copyText(third, view::CopyForm::ValueAndBound, x), "1/3");  // exact: there is no bound
    EXPECT_EQ(view::copyText(evaluated("-7/4", NumberType::Exact), view::CopyForm::Value, x), "-7/4");
    EXPECT_EQ(view::copyText(evaluated("6", NumberType::Exact), view::CopyForm::Value, x), "6");
}

TEST(Presenter, DetailsCopyAsLines) {
    const QStringList lines =
        view::copyText(evaluated("0.1 + 0.2"), view::CopyForm::Details, typeInfo(NumberType::Double)).split('\n');
    EXPECT_EQ(lines.value(0), "0.1 + 0.2 = 0.3000000000000000444089209850062616169452667236328125");
    EXPECT_TRUE(lines.contains("Guaranteed bound: 4.4e-17"));
    EXPECT_TRUE(lines.contains("Trusted digits: 15 by the bound, 15 by the measurement"));
    EXPECT_TRUE(lines.contains("Number type: double, 53-bit significand"));
    EXPECT_FALSE(lines.contains("Evaluated: 0.1 + 0.2"));  // already the first line
    EXPECT_EQ(view::copyText(evaluate("1/0"), view::CopyForm::Details, typeInfo(NumberType::Double)), "");
}

TEST(Presenter, ExactDetailsCopyAsLines) {
    const QStringList lines =
        view::copyText(evaluated("-7/4", NumberType::Exact), view::CopyForm::Details, typeInfo(NumberType::Exact)).split('\n');
    EXPECT_EQ(lines.value(0), "-7/4 = -7/4");
    EXPECT_TRUE(lines.contains("Error: exact · no rounding error"));
    EXPECT_TRUE(lines.contains("Number type: cpp_rational, exact fractions"));
}

TEST(Presenter, WithADecimalCommaResultsUseIt) {
    view::ValueParts p = view::valueParts(evaluated("0.1 + 0.2"));
    EXPECT_EQ(view::withDecimalComma(p).trusted, "0,300000000000000");
    EXPECT_EQ(view::withDecimalComma(QStringLiteral("nCr(1.5, 2)")), "nCr(1,5; 2)");
    settings::setDecimalComma(true);
    EXPECT_EQ(view::copyText(evaluated("0.1 + 0.2"), view::CopyForm::Trusted, typeInfo(NumberType::Double)), "0,300000000000000");
    settings::setDecimalComma(false);
}

TEST(Presenter, PastedTextWithADecimalCommaReadsBack) {
    settings::setDecimalComma(true);
    Entry e;
    e.setRoot(typing::read("nCr(1,5; 2)"));
    EXPECT_EQ(e.text(), "nCr(1.5, 2)");
    settings::setDecimalComma(false);
}

TEST(Presenter, WithADecimalCommaErrorsQuoteTheInputAsShown) {
    settings::setDecimalComma(true);
    const Error unexpected = *evaluate("3.5, ").error;
    EXPECT_EQ(view::errorText(unexpected, "3.5, "), "Unexpected “;”");
    const Error whole = *evaluated("nCr(1.5, 2)").error;
    EXPECT_TRUE(view::errorText(whole, "nCr(1.5, 2)").startsWith("nCr(1,5; 2)")) << view::errorText(whole, "nCr(1.5, 2)").toStdString();
    settings::setDecimalComma(false);
    EXPECT_EQ(view::errorText(unexpected, "3.5, "), "Unexpected “,”");
}

TEST(Presenter, ThePercentageQuestionsAreExpressions) {
    const QList<view::PercentageRow> rows = view::percentageRows(QStringLiteral("80"), QStringLiteral("100"));
    QStringList keys, expressions;
    for (const view::PercentageRow& row : rows) {
        keys << row.key;
        expressions << row.expression;
        EXPECT_FALSE(row.title.isEmpty()) << row.key.toStdString();
    }
    EXPECT_EQ(keys, QStringList({"change", "changeBack", "secondOfFirst", "firstOfSecond", "plus", "minus", "of"}));
    EXPECT_EQ(expressions, QStringList({"((100)−(80))÷(80)×100", "((80)−(100))÷(100)×100", "(100)÷(80)×100",
                                        "(80)÷(100)×100", "(80)+(80)×(100)÷100", "(80)−(80)×(100)÷100", "(80)×(100)÷100"}));
    QStringList values;
    for (const view::PercentageRow& row : rows) {
        const Result r = evaluated(row.expression.toUtf8().constData(), NumberType::Exact);
        ASSERT_FALSE(r.error) << row.key.toStdString();
        values << QString::fromStdString((r.exact->negative ? "-" : "") + r.exact->numerator + "/" + r.exact->denominator);
    }
    EXPECT_EQ(values, QStringList({"25/1", "-20/1", "125/1", "80/1", "160/1", "0/1", "80/1"}));
    EXPECT_TRUE(view::percentageRows(QStringLiteral(""), QStringLiteral("100")).isEmpty());  // a value is missing
}

TEST(Presenter, ANoteHasNoDetails) {
    const Result note = evaluated("# a note");
    EXPECT_TRUE(view::details(note, typeInfo(NumberType::Double)).isEmpty());
    EXPECT_EQ(view::valueParts(note).trusted, "");
}
