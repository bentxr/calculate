#include "plotfonts.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QDir>

#include <gtest/gtest.h>

namespace {

// A figure with text in matplotlib's default face and in its mathematical faces.
constexpr const char* kFigure =
    R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
        {"on": "ax", "call": "plot", "args": [[0, 1, 2], [0, 1, 4]], "kwargs": {"label": "$x^2$"}},
        {"on": "ax", "call": "set_title", "args": ["Squares"]}, {"on": "ax", "call": "legend"}]})";

}  // namespace

// The plots' fonts ship inside the program: matplotlib's whole set, under :/plotfonts.
TEST(PlotFonts, AreAllEmbedded) {
    const QStringList files = QDir(QStringLiteral(":/plotfonts")).entryList(QDir::Files);
    const QStringList source = QDir(QStringLiteral(CALCULATE_PLOT_FONTS)).entryList({QStringLiteral("*.ttf")}, QDir::Files);
    EXPECT_EQ(files.size(), 40);
    EXPECT_EQ(files, source);
}

// The library draws through the program's link with the embedded fonts, and draws the same pixels as from
// its own font files.
TEST(PlotFonts, TheLibraryDrawsWithTheEmbeddedFonts) {
    std::string error;
    installPlotFonts();
    const calculate_plot::Image embedded = calculate_plot::drawScreen(kFigure, 1.0, std::string(), &error);
    EXPECT_EQ(error, "");
    EXPECT_EQ(embedded.width, 640);
    EXPECT_EQ(embedded.height, 480);

    calculate_plot::useFontBytes(nullptr);
    const calculate_plot::Image none = calculate_plot::drawScreen(kFigure, 1.0, std::string(), &error);
    EXPECT_NE(error, "");  // without them there is no text to draw
    error.clear();
    const calculate_plot::Image files = calculate_plot::drawScreen(kFigure, 1.0, CALCULATE_PLOT_FONTS, &error);
    EXPECT_EQ(error, "");
    EXPECT_EQ(embedded.rgba, files.rgba);
    installPlotFonts();
}

// Installing them again keeps the bytes already read (they stay valid for drawings under way).
TEST(PlotFonts, AreReadOnce) {
    const QByteArray* first = plotFontBytes(QStringLiteral("DejaVuSans.ttf"));
    ASSERT_NE(first, nullptr);
    installPlotFonts();
    EXPECT_EQ(plotFontBytes(QStringLiteral("DejaVuSans.ttf")), first);
    EXPECT_GT(first->size(), 100000);
    EXPECT_EQ(plotFontBytes(QStringLiteral("NoSuchFont.ttf")), nullptr);
}
