#include "presenter.hpp"

#include "printers.hpp"

#include <gtest/gtest.h>

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

TEST(Presenter, TheThreeDigitStylesNeverRelyOnColourAlone) {
    const Result r = evaluated("0.1 + 0.2");
    EXPECT_EQ(view::valueHtml(r, view::DigitStyle::Faded, "#888888"),
              "0.300000000000000<span style=\"color:#888888\"><i>0444089209850062616169452667236328125</i></span>");
    EXPECT_EQ(view::valueHtml(r, view::DigitStyle::Bold, "#888888"),
              "<b>0.300000000000000</b><span style=\"color:#888888\">0444089209850062616169452667236328125</span>");
    EXPECT_EQ(view::valueHtml(r, view::DigitStyle::Bar, "#888888"),
              "0.300000000000000<span style=\"color:#888888\">|0444089209850062616169452667236328125</span>");
}

TEST(Presenter, ValuesWithoutNoiseAndLargeValues) {
    EXPECT_EQ(view::valueHtml(evaluated("2+2"), view::DigitStyle::Faded, "#888888"), "4");
    EXPECT_EQ(view::valueHtml(evaluated("-1e30"), view::DigitStyle::Bold, "#888888"),
              "<b>−1.000000000000000</b><span style=\"color:#888888\">019884624838656</span> × 10<sup>30</sup>");
    EXPECT_EQ(view::valueHtml(evaluated("1/3", NumberType::Exact), view::DigitStyle::Faded, "#888888"), "1/3 = 0.(3)");
}

TEST(Presenter, TheErrorLineAndTheWhyLine) {
    const Result r = evaluated("0.1 + 0.2");
    EXPECT_EQ(view::errorLine(r), "± 4.4e-17 · 15 trusted digits");
    EXPECT_EQ(view::whyLine(r, typeInfo(NumberType::Double)),
              "input 1.7e-17 · rounding 2.8e-17 · library 0 · rounded operations: 1 · double, 53-bit significand");
    EXPECT_EQ(view::errorLine(evaluated("2+2")), "± 0 · all digits trusted");
    EXPECT_EQ(view::errorLine(evaluated("1/3", NumberType::Exact)), "exact · no rounding error");
    EXPECT_EQ(view::whyLine(evaluated("1/3", NumberType::Exact), typeInfo(NumberType::Exact)), "");
    Options allow;
    allow.allowUncertainDiscreteArguments = true;
    EXPECT_EQ(view::errorLine(evaluate("(0.1*30)!", allow)),
              "± 0 · all digits trusted · incomplete: an uncertain argument was accepted");
}
