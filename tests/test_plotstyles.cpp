#include "ploteditor.hpp"
#include "plotstyles.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// The style chooser (D9.8): default and matplotlib's bundled styles, by name in a list and as thumbnails of the
// figure drawn by the engine in each.
namespace {

const std::string kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},)"
                            R"( {"on": "ax", "call": "plot", "args": [[0, 1, 2], [0, 1, 4]]}]})";

std::string styled(const std::string& figure, const std::string& style) {
    std::string error;
    const std::string out = calculate_plot::setStyle(figure, style, &error);
    EXPECT_EQ(error, "");
    return out;
}

struct Editor {
    PlotEditor editor;
    QByteArray edited;
    explicit Editor(const std::string& figure = kFigure) {
        editor.resize(360, 700);
        editor.setFigure(QByteArray::fromStdString(figure));
        QObject::connect(&editor, &PlotEditor::figureEdited, &editor, [this](const QByteArray& f) { edited = f; });
        editor.show();
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&editor));
    }
};

}  // namespace

TEST(PlotStyles, TheListHasDefaultAndTheBundledStyles) {
    Editor e(styled(kFigure, R"("bmh")"));
    auto* list = e.editor.findChild<QComboBox*>("plotStyle");
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->count(), 32);
    EXPECT_EQ(list->itemText(0), "default");
    EXPECT_EQ(list->currentText(), "bmh");  // what the file says
    list->setCurrentIndex(list->findText("ggplot"));
    emit list->activated(list->currentIndex());
    EXPECT_EQ(e.edited, QByteArray::fromStdString(styled(kFigure, R"("ggplot")")));
    list->setCurrentIndex(0);
    emit list->activated(0);
    EXPECT_EQ(e.edited, QByteArray::fromStdString(styled(kFigure, "")));
}

// Choose... shows each style's thumbnail: the figure drawn by the engine in it; a click takes it.
TEST(PlotStyles, TheChooserShowsThumbnailsDrawnByTheEngine) {
    PlotStylesDialog dialog(QByteArray::fromStdString(kFigure), nullptr);
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));
    QList<QToolButton*> tiles;
    for (const QString& name : QStringList{"default", "ggplot", "dark_background", "Solarize_Light2"}) {
        auto* tile = dialog.findChild<QToolButton*>("style:" + name);
        ASSERT_NE(tile, nullptr) << name.toStdString();
        tiles << tile;
    }
    EXPECT_EQ(dialog.findChildren<QToolButton*>(QRegularExpression("^style:")).size(), 32);
    ASSERT_TRUE(QTest::qWaitFor([&] { return dialog.finished(); }, 30000));
    for (QToolButton* tile : dialog.findChildren<QToolButton*>(QRegularExpression("^style:")))  // every style drawn
        EXPECT_FALSE(dialog.thumbnail(tile->objectName().mid(6)).isNull()) << tile->objectName().toStdString();
    // a true miniature: the figure as a 640 x 480 canvas shows it, drawn at a quarter of the screen's ratio (the
    // text keeps its place instead of being squeezed into the tile)
    std::string error;
    const auto want = calculate_plot::drawCanvas(styled(kFigure, R"("ggplot")"),
                                                 calculate_plot::Canvas{640, 480, 0.25 * dialog.devicePixelRatioF()},
                                                 std::string(CALCULATE_PLOT_FONTS), &error);
    ASSERT_EQ(error, "");
    const QImage thumbnail = dialog.thumbnail("ggplot");
    EXPECT_EQ(thumbnail.deviceIndependentSize().toSize(), dialog.thumbnailSize());
    const QImage got = thumbnail.convertToFormat(QImage::Format_RGBA8888);
    ASSERT_EQ(got.width(), want.width);
    EXPECT_EQ(QByteArray(reinterpret_cast<const char*>(got.constBits()), qsizetype(got.sizeInBytes())),
              QByteArray(reinterpret_cast<const char*>(want.rgba.data()), qsizetype(want.rgba.size())));
    EXPECT_FALSE(tiles[1]->icon().isNull());
    tiles[2]->click();
    EXPECT_EQ(dialog.chosen(), "dark_background");
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

// From the editor: Choose... then a tile sets the style.
TEST(PlotStyles, TheEditorOpensTheChooser) {
    Editor e;
    auto* choose = e.editor.findChild<QPushButton*>("plotChooseStyle");
    ASSERT_NE(choose, nullptr);
    QTimer::singleShot(0, [] {
        auto* dialog = qApp->findChild<PlotStylesDialog*>();
        for (QWidget* w : QApplication::topLevelWidgets())
            if (auto* d = qobject_cast<PlotStylesDialog*>(w)) dialog = d;
        ASSERT_NE(dialog, nullptr);
        dialog->findChild<QToolButton*>("style:fast")->click();
    });
    choose->click();
    EXPECT_EQ(e.edited, QByteArray::fromStdString(styled(kFigure, R"("fast")")));
}
