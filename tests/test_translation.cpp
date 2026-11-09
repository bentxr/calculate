#include "constanttext.hpp"
#include "functiontext.hpp"
#include "keypad.hpp"
#include "presenter.hpp"
#include "printers.hpp"
#include "settings.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QLocale>
#include <QTranslator>

#include <gtest/gtest.h>

#include <memory>

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

// Spanish quotes with « »: no translation keeps the English “ ”.
TEST(Translation, SpanishQuotesWithGuillemets) {
    QFile ts(CALCULATE_TS_FILE);
    ASSERT_TRUE(ts.open(QIODevice::ReadOnly));
    for (const QString& line : QString::fromUtf8(ts.readAll()).split('\n'))
        if (line.contains("<translation>")) {
            EXPECT_FALSE(line.contains(QChar(0x201C)) || line.contains(QChar(0x201D))) << line.trimmed().toStdString();
        }
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

// The Spanish texts as the review settled them: every constant's title capitalized and without a raw "_"; the inverse
// functions "El x tal que"; floor and ceil said plainly; division said "entre", never "dividida por"; no backticks.
TEST(Translation, SpanishTextsReadWell) {
    QTranslator spanish;
    ASSERT_TRUE(spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n"));
    QCoreApplication::installTranslator(&spanish);
    for (const calculate_core::ConstantDescription& c : calculate_core::constants()) {
        const QString title = constantTitle(QString::fromStdString(c.name));
        ASSERT_FALSE(title.isEmpty()) << c.name;
        EXPECT_FALSE(title[0].isLower()) << c.name << ": " << title.toStdString();
        EXPECT_FALSE(title.contains('_')) << c.name << ": " << title.toStdString();
    }
    for (const calculate_core::FunctionDescription& f : calculate_core::functions()) {
        const QString text = functionDescription(f);
        EXPECT_FALSE(text.contains("dividida por") || text.contains("dividido por")) << f.name;
        EXPECT_FALSE(text.contains('`')) << f.name;
        if (f.name == "erfinv" || f.name == "erfcinv" || f.name == "betaincinv") {
            EXPECT_TRUE(text.startsWith("El x tal que ")) << text.toStdString();
        }
        if (f.name == "floor") {
            EXPECT_EQ(text, "El mayor entero menor o igual que x.");
        }
        if (f.name == "ceil") {
            EXPECT_EQ(text, "El menor entero mayor o igual que x.");
        }
    }
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

// Qt's own texts (a text field's context menu, the standard buttons of dialogs) follow the language too: the app
// carries Qt's Spanish translation and installs it beside its own.
TEST(Translation, QtsOwnTextsFollowTheLanguage) {
    const char* context = "QPlatformTheme";  // Qt's, not ours: through variables, so lupdate leaves them out
    const char* cancel = "Cancel";
    settings::setLanguage(settings::Language::Spanish);
    QLineEdit field;
    std::unique_ptr<QMenu> menu(field.createStandardContextMenu());
    EXPECT_TRUE(menu->actions().first()->text().startsWith(QStringLiteral("&Deshacer"))) << menu->actions().first()->text().toStdString();
    EXPECT_EQ(QCoreApplication::translate(context, cancel), QStringLiteral("Cancelar"));
    settings::setLanguage(settings::Language::English);
    std::unique_ptr<QMenu> english(field.createStandardContextMenu());
    EXPECT_TRUE(english->actions().first()->text().startsWith(QStringLiteral("&Undo")));
    EXPECT_EQ(QCoreApplication::translate(context, cancel), QStringLiteral("Cancel"));
}
