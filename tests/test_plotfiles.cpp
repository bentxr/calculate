#include "filedialogs.hpp"
#include "plotcanvas.hpp"
#include "plotdata.hpp"
#include "ploteditor.hpp"
#include "plotpage.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// Open and export (D9.9): the figure saved as PNG, SVG, a Python script or the figure file itself, a figure file
// opened, and data imported into the Data tab, through the app's file dialogs (a download and the file picker in
// the browser). The tests stand in for the dialogs.
namespace {

const QByteArray kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
    {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]]}, {"on": "ax", "call": "set_title", "args": ["Squares"]}]})";

std::string text(const QByteArray& b) { return std::string(b.constData(), std::size_t(b.size())); }

// The dialogs as the test answers them: what a save was given, and what an open returns.
struct Dialogs {
    QByteArray saved;
    QString savedName;
    QString openFilter;
    QString openName;
    QByteArray openContent;
    Dialogs() {
        filedialogs::setSave([this](const QByteArray& content, const QString& name, QWidget*) {
            saved = content;
            savedName = name;
        });
        filedialogs::setOpen([this](const QString& filter, const filedialogs::Opened& opened, QWidget*) {
            openFilter = filter;
            opened(openName, openContent);
        });
    }
    ~Dialogs() {
        filedialogs::setSave(nullptr);
        filedialogs::setOpen(nullptr);
    }
};

struct Page {
    PlotPage page;
    PlotCanvas* canvas = nullptr;
    Page() {
        page.resize(1000, 650);
        page.setNarrow(false);
        page.show();
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&page));
        canvas = page.canvas();
        canvas->setFigure(kFigure);
        settle();
    }
    bool settle() {
        return QTest::qWaitFor([&] { return !canvas->busy() && canvas->image().size() == canvas->size() * canvas->devicePixelRatioF(); }, 10000);
    }
    QAction* action(const char* name) { return page.findChild<QAction*>(name); }
};

}  // namespace

// Save offers each format; each gives what the library writes for the figure, under a name with its extension.
TEST(PlotFiles, SaveOffersEveryFormat) {
    Dialogs d;
    Page p;
    auto* save = p.page.findChild<QToolButton*>("plotSave");
    ASSERT_NE(save, nullptr);
    ASSERT_NE(save->menu(), nullptr);
    EXPECT_EQ(save->popupMode(), QToolButton::InstantPopup);
    std::string error;
    const std::string png = calculate_plot::drawPng(text(kFigure), std::string(CALCULATE_PLOT_FONTS), &error);
    const std::string svg = calculate_plot::drawSvg(text(kFigure), std::string(CALCULATE_PLOT_FONTS), &error);
    const std::string py = calculate_plot::exportPython(text(kFigure), &error);
    ASSERT_EQ(error, "");
    const struct {
        const char* action;
        QByteArray content;
        QString name;
    } formats[] = {{"save:png", QByteArray::fromStdString(png), "figure.png"},
                   {"save:svg", QByteArray::fromStdString(svg), "figure.svg"},
                   {"save:py", QByteArray::fromStdString(py), "figure.py"},
                   {"save:figure", kFigure, "figure.json"}};
    for (const auto& f : formats) {
        QAction* a = p.action(f.action);
        ASSERT_NE(a, nullptr) << f.action;
        EXPECT_TRUE(save->menu()->actions().contains(a)) << f.action;
        EXPECT_FALSE(a->text().isEmpty()) << f.action;
        d.saved.clear();
        a->trigger();
        EXPECT_EQ(d.savedName, f.name);
        EXPECT_TRUE(d.saved == f.content) << f.action;
    }
}

// The save key (ctrl+s) opens the same choice of formats.
TEST(PlotFiles, TheSaveKeyOpensTheFormats) {
    Dialogs d;
    Page p;
    auto* save = p.page.findChild<QToolButton*>("plotSave");
    p.canvas->setFocus();
    QTest::keyClick(p.canvas, Qt::Key_S, Qt::ControlModifier);
    EXPECT_TRUE(QTest::qWaitFor([&] { return save->menu()->isVisible(); }, 2000));
    save->menu()->hide();
}

// Open takes a figure file: it becomes the document of the canvas and the editor. A file that is not one is refused
// with the library's reason, and the document stays.
TEST(PlotFiles, OpenTakesAFigureFile) {
    Dialogs d;
    Page p;
    auto* open = p.page.findChild<QToolButton*>("plotOpen");
    ASSERT_NE(open, nullptr);
    EXPECT_FALSE(open->toolTip().isEmpty());
    const QByteArray other = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
        {"on": "ax", "call": "bar", "args": [[1, 2, 3], [3, 1, 2]]}]})";
    d.openName = "bars.json";
    d.openContent = other;
    open->click();
    EXPECT_TRUE(d.openFilter.contains("*.json"));
    EXPECT_EQ(p.canvas->figure(), other);
    auto* editor = p.page.findChild<PlotEditor*>("plotEditor");
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->figure(), other);
    ASSERT_TRUE(p.settle());
    // not a figure file: refused, with why
    d.openName = "notes.json";
    d.openContent = R"({"version": 1, "calls": [{"call": "bogus"}]})";
    open->click();
    EXPECT_EQ(p.canvas->figure(), other);
    auto* line = p.page.findChild<QLabel*>("plotCoordinates");
    EXPECT_TRUE(line->text().contains("notes.json")) << line->text().toStdString();
    // cancelled: nothing happens
    d.openName.clear();
    d.openContent.clear();
    open->click();
    EXPECT_EQ(p.canvas->figure(), other);
    // the keyboard: ctrl+O
    d.openName = "again.json";
    d.openContent = kFigure;
    p.canvas->setFocus();
    QTest::keyClick(p.canvas, Qt::Key_O, Qt::ControlModifier);
    EXPECT_EQ(p.canvas->figure(), kFigure);
}

// The Data tab's Import… reads a CSV / TSV file into a new line series.
TEST(PlotFiles, ImportReadsDataIntoANewSeries) {
    Dialogs d;
    Page p;
    auto* import = p.page.findChild<QPushButton*>("plotImport");
    ASSERT_NE(import, nullptr);
    EXPECT_FALSE(import->text().isEmpty());
    d.openName = "points.csv";
    d.openContent = "x;y\n1;2.5\n2;3.5\n4;-1\n";
    import->click();
    EXPECT_TRUE(d.openFilter.contains("*.csv"));
    EXPECT_NE(p.canvas->figure().indexOf(R"("args":[[1,2,4],[2.5,3.5,-1]])"), -1) << p.canvas->figure().toStdString();
    // cancelled: nothing happens
    const QByteArray before = p.canvas->figure();
    d.openName.clear();
    d.openContent.clear();
    import->click();
    EXPECT_EQ(p.canvas->figure(), before);
}

// A figure that cannot be drawn is not saved as an image: the library's reason shows instead.
TEST(PlotFiles, AFigureThatCannotBeDrawnIsNotSaved) {
    Dialogs d;
    Page p;
    p.canvas->setFigure(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}, {"on": "ax", "call": "plot", "args": [[1, 2], [1]]}]})");
    auto* line = p.page.findChild<QLabel*>("plotCoordinates");
    for (const char* format : {"save:png", "save:svg"}) {
        d.savedName.clear();
        line->clear();
        p.action(format)->trigger();
        EXPECT_EQ(d.savedName, "") << format;
        EXPECT_FALSE(line->text().isEmpty()) << format;
    }
}
