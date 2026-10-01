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
    EXPECT_EQ(view::errorLine(evaluate("0.1 + 0.2")), "± 4.4e-17 · 15 dígitos fiables");
    EXPECT_EQ(translated("asin"), "Arcsen");  // the keys read as on a Spanish calculator
    EXPECT_EQ(translated("gcd("), "MCD(");
    EXPECT_EQ(translated("asinh("), "Arcsenh(");
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
