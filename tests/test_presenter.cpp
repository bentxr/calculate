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
