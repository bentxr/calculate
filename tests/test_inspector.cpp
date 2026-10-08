#include "inspector.hpp"

#include "printers.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTest>

#include <gtest/gtest.h>

using namespace calculate_core;

namespace {

template <class W>
W* child(QWidget& parent, const QString& name) {
    W* w = parent.findChild<W*>(name);
    EXPECT_NE(w, nullptr) << name.toStdString();
    return w;
}

QString text(Inspector& inspector, const char* field) { return child<QPlainTextEdit>(inspector, field)->toPlainText(); }

QString shown(Inspector& inspector, const char* key) { return child<QLabel>(inspector, QString("inspector:") + key)->text(); }

}  // namespace

TEST(Inspector, ListsTheFormats) {
    Inspector inspector;
    auto* format = child<QComboBox>(inspector, "inspectorFormat");
    EXPECT_EQ(format->count(), static_cast<int>(floatFormats().size()));
    EXPECT_EQ(format->currentIndex(), inspector.formatIndex(NumberType::Double));
    EXPECT_TRUE(format->itemText(0).startsWith("binary16"));
}

TEST(Inspector, ConvertsADecimal) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    inspector.setDecimal("0.1");
    EXPECT_EQ(text(inspector, "inspectorBinary"), "0  0111 1011  1001 1001 1001 1001 1001 101");
    EXPECT_EQ(text(inspector, "inspectorHex"), "3DCC CCCD");
    EXPECT_EQ(shown(inspector, "class"), "normal");
    EXPECT_EQ(shown(inspector, "parts"), "sign + · exponent −4 (field 123) · significand 1.60000002384185791015625");
    EXPECT_EQ(shown(inspector, "value"), "0.100000001490116119384765625");
    EXPECT_EQ(shown(inspector, "error"), "+1.490116119384765625e−9");
    EXPECT_EQ(shown(inspector, "ulp"), "2^−27 = 7.450580596923828125e−9");
    EXPECT_EQ(shown(inspector, "below"), "0.0999999940395355224609375");
    EXPECT_EQ(shown(inspector, "above"), "0.10000000894069671630859375");
}
