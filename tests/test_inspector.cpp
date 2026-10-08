#include "inspector.hpp"

#include "printers.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTest>
#include <QToolButton>

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

TEST(Inspector, EditingTheHexUpdatesTheOthers) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    auto* hex = child<QPlainTextEdit>(inspector, "inspectorHex");
    hex->selectAll();
    QTest::keyClicks(hex, "3F800000");
    EXPECT_EQ(hex->toPlainText(), "3F800000");  // the field being typed in stays as typed
    EXPECT_EQ(text(inspector, "inspectorDecimal"), "1");
    EXPECT_EQ(text(inspector, "inspectorBinary"), "0  0111 1111  0000 0000 0000 0000 0000 000");
    EXPECT_EQ(shown(inspector, "error"), "");  // a pattern, not a conversion
}

TEST(Inspector, EditingTheBitsUpdatesTheOthers) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    auto* binary = child<QPlainTextEdit>(inspector, "inspectorBinary");
    binary->selectAll();
    QTest::keyClicks(binary, "01000000010000000000000000000000");
    EXPECT_EQ(text(inspector, "inspectorHex"), "4040 0000");
    EXPECT_EQ(text(inspector, "inspectorDecimal"), "3");
}

TEST(Inspector, AnUnreadableFieldLeavesTheOthers) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    inspector.setDecimal("0.1");
    auto* decimal = child<QPlainTextEdit>(inspector, "inspectorDecimal");
    decimal->selectAll();
    QTest::keyClicks(decimal, "0.1.");
    EXPECT_FALSE(child<QLabel>(inspector, "inspectorMessage")->text().isEmpty());
    EXPECT_EQ(text(inspector, "inspectorHex"), "3DCC CCCD");
}

TEST(Inspector, FieldsTakeOnlyTheirCharacters) {
    Inspector inspector;
    auto* hex = child<QPlainTextEdit>(inspector, "inspectorHex");
    hex->clear();
    QTest::keyClicks(hex, "3z!");
    EXPECT_EQ(hex->toPlainText(), "3");
    QTest::keyClick(hex, Qt::Key_Return);
    EXPECT_EQ(hex->toPlainText(), "3");
}

TEST(Inspector, StepsToTheNeighbours) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Double));
    inspector.setDecimal("0.1");
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorUp"), Qt::LeftButton);
    EXPECT_EQ(text(inspector, "inspectorHex"), "3FB9 9999 9999 999B");
    EXPECT_EQ(text(inspector, "inspectorDecimal"), "0.10000000000000001942890293094023945741355419158935546875");
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorDown"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorDown"), Qt::LeftButton);
    EXPECT_EQ(text(inspector, "inspectorHex"), "3FB9 9999 9999 9999");
}

TEST(Inspector, ChangingTheFormatConvertsTheDecimalAgain) {
    Inspector inspector;
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    inspector.setDecimal("0.1");
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Double));
    EXPECT_EQ(text(inspector, "inspectorHex"), "3FB9 9999 9999 999A");
    EXPECT_EQ(text(inspector, "inspectorDecimal"), "0.1");
}

TEST(Inspector, NoStepsWhereThereIsNoNeighbour) {
    Inspector inspector;
    inspector.setDecimal("nan");
    EXPECT_FALSE(child<QPushButton>(inspector, "inspectorUp")->isEnabled());
    EXPECT_FALSE(child<QPushButton>(inspector, "inspectorDown")->isEnabled());
}

TEST(Inspector, AValueWithoutDecimalsIsRereadFromItsBits) {
    Inspector inspector;
    inspector.loadBits(inspector.formatIndex(NumberType::Binary512), "1");  // 2^-4194790: no decimal written out
    ASSERT_TRUE(text(inspector, "inspectorDecimal").contains("2^"));
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    EXPECT_EQ(text(inspector, "inspectorHex"), "0000 0001");
    EXPECT_FALSE(child<QLabel>(inspector, "inspectorMessage")->text().isEmpty());  // says the bits were reread
}

TEST(Inspector, ClickingABitFlipsIt) {
    Inspector inspector;
    inspector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&inspector));
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    inspector.setDecimal("1");
    auto* strip = child<BitStrip>(inspector, "inspectorBitStrip");
    ASSERT_EQ(strip->bitCount(), 32);
    QTest::mouseClick(strip, Qt::LeftButton, {}, strip->bitCenter(0));  // the sign bit
    EXPECT_EQ(text(inspector, "inspectorHex"), "BF80 0000");
    EXPECT_EQ(text(inspector, "inspectorDecimal"), "-1");
    QTest::mouseClick(strip, Qt::LeftButton, {}, strip->bitCenter(31));  // the last fraction bit
    EXPECT_EQ(text(inspector, "inspectorHex"), "BF80 0001");
    QTest::mouseClick(strip, Qt::LeftButton, {}, strip->bitCenter(0));
    EXPECT_EQ(text(inspector, "inspectorHex"), "3F80 0001");
    EXPECT_FALSE(strip->accessibleName().isEmpty());
}

TEST(Inspector, TheBitsWaitWhileAFieldIsUnreadable) {
    Inspector inspector;
    inspector.setDecimal("0.1");
    auto* strip = child<BitStrip>(inspector, "inspectorBitStrip");
    EXPECT_TRUE(strip->isEnabled());
    auto* decimal = child<QPlainTextEdit>(inspector, "inspectorDecimal");
    decimal->selectAll();
    QTest::keyClicks(decimal, "0.1.");
    EXPECT_FALSE(strip->isEnabled());
    inspector.setDecimal("");
    EXPECT_EQ(strip->bitCount(), 0);
}

TEST(Inspector, AKeypadTypesIntoTheLastField) {
    Inspector inspector;
    inspector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&inspector));
    inspector.setFormatIndex(inspector.formatIndex(NumberType::Float));
    auto* hex = child<QPlainTextEdit>(inspector, "inspectorHex");
    hex->setFocus();
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorKey:3"), Qt::LeftButton);
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorKey:F"), Qt::LeftButton);
    EXPECT_EQ(hex->toPlainText(), "3F");
    QTest::mouseClick(child<QPushButton>(inspector, "inspectorKey:⌫"), Qt::LeftButton);
    EXPECT_EQ(hex->toPlainText(), "3");
    auto* keys = child<QWidget>(inspector, "inspectorKeys");
    EXPECT_TRUE(keys->isVisible());
    QTest::mouseClick(child<QToolButton>(inspector, "inspectorKeysToggle"), Qt::LeftButton);
    EXPECT_FALSE(keys->isVisible());
}
