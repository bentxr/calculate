#include "constanttext.hpp"
#include "functiontext.hpp"
#include "keypad.hpp"
#include "presenter.hpp"
#include "printers.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QTranslator>

#include <gtest/gtest.h>

using namespace calculate_core;

TEST(Translation, SpanishIsEmbeddedAndUsed) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    QCoreApplication::installTranslator(&spanish);
    EXPECT_EQ(view::errorText(*evaluate("1/0").error, "1/0"), "División por cero");
    EXPECT_EQ(view::typeLabel(numberTypes()[1]), "Doble · double · 64 bits · ~16 dígitos");
    EXPECT_EQ(view::details(evaluate("0.1 + 0.2"), numberTypes()[1])[2].value, "15 según la cota, 15 según la medición");
    EXPECT_EQ(translated("asin"), "arcsen");  // the keys read as on a Spanish calculator
    EXPECT_EQ(translated("gcd("), "mcd(");
    EXPECT_EQ(translated("asinh("), "arcsenh(");
    QCoreApplication::removeTranslator(&spanish);
    EXPECT_EQ(view::errorText(*evaluate("1/0").error, "1/0"), "Division by zero");
}

TEST(Translation, EveryStringIsTranslated) {
    QFile ts(CALCULATE_TS_FILE);
    ASSERT_TRUE(ts.open(QIODevice::ReadOnly));
    const QByteArray content = ts.readAll();
    EXPECT_FALSE(content.contains("type=\"unfinished\""));
    EXPECT_FALSE(content.contains("<translation></translation>"));
}

TEST(Translation, EveryFunctionTextIsMarkedForTranslation) {
    const QStringList marked = functionTextsForTranslation();
    for (const calculate_core::FunctionDescription& f : calculate_core::functions()) {
        EXPECT_TRUE(marked.contains(QString::fromStdString(f.title))) << f.name;
        EXPECT_TRUE(marked.contains(QString::fromStdString(f.description))) << f.name;
    }
}

TEST(Translation, FunctionTextsReadInSpanish) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    QCoreApplication::installTranslator(&spanish);
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        if (f.name == "atan2") EXPECT_EQ(functionTitle(f), QStringLiteral("Ángulo de un punto"));
    QCoreApplication::removeTranslator(&spanish);
}

TEST(Translation, EveryConstantTitleIsMarkedForTranslation) {
    const QStringList marked = constantTextsForTranslation();
    for (const calculate_core::ConstantDescription& c : calculate_core::constants())
        EXPECT_TRUE(marked.contains(QString::fromStdString(c.title))) << c.name;
}

TEST(Translation, TheInspectorInSpanish) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    QCoreApplication::installTranslator(&spanish);
    EXPECT_EQ(view::floatClassName(FloatClass::QuietNaN), "NaN silencioso");
    const QList<view::DetailRow> rows = view::storedRows(evaluate("0.5"), view::bitColours(false));
    EXPECT_EQ(rows[0].label, "Bits almacenados");
    EXPECT_EQ(rows[4].label, "Anterior representable");
    QCoreApplication::removeTranslator(&spanish);
}
