#include "plotcanvas.hpp"
#include "ploteditor.hpp"
#include "plotpage.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// The plot page on a phone (D9.10, D9.14): the canvas takes the width, the navigation row sits under it, and under
// that a bar opens the editor as a sheet (Data, Style, or Style with the search). The open sheet lies over the lower
// part of the canvas instead of pushing it off the screen; its handle, or Escape, closes it.
namespace {

const QByteArray kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
    {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]]}]})";

// The widget's rectangle in the page's coordinates.
QRect placed(QWidget* w, QWidget* page) { return QRect(w->mapTo(page, QPoint(0, 0)), w->size()); }

struct Phone {
    PlotPage page;
    PlotCanvas* canvas = nullptr;
    PlotEditor* editor = nullptr;
    QWidget* sheet = nullptr;
    Phone() {
        page.resize(390, 760);
        page.setNarrow(true);
        page.show();
        EXPECT_TRUE(QTest::qWaitForWindowExposed(&page));
        canvas = page.canvas();
        canvas->setFigure(kFigure);
        editor = page.findChild<PlotEditor*>("plotEditor");
        sheet = page.findChild<QWidget*>("plotSheet");
        EXPECT_NE(editor, nullptr);
        EXPECT_NE(sheet, nullptr);
        QApplication::processEvents();
    }
    template <class W>
    W* child(const char* name) {
        W* w = page.findChild<W*>(name);
        EXPECT_NE(w, nullptr) << name;
        return w;
    }
    QRect at(QWidget* w) { return placed(w, &page); }
};

}  // namespace

TEST(PlotPhone, TheNavigationRowAndTheSheetBarAreUnderTheCanvas) {
    Phone p;
    auto* tools = p.child<QWidget>("plotTools");
    auto* bar = p.child<QWidget>("plotSheetBar");
    EXPECT_FALSE(p.editor->isVisible());
    EXPECT_FALSE(p.sheet->isVisible());
    EXPECT_TRUE(bar->isVisible());
    EXPECT_GE(p.at(p.canvas).width(), p.page.width() - 20);  // the canvas takes the width
    EXPECT_LE(p.at(p.canvas).top(), 10);
    EXPECT_GE(p.at(tools).top(), p.at(p.canvas).bottom());
    EXPECT_GE(p.at(bar).top(), p.at(tools).bottom());
    EXPECT_GE(p.at(bar).bottom(), p.page.height() - 10);
    for (const char* name : {"sheet:data", "sheet:style", "sheet:find"}) EXPECT_FALSE(p.child<QPushButton>(name)->text().isEmpty()) << name;
}

TEST(PlotPhone, TheSheetOpensOverTheLowerPartOfTheCanvas) {
    Phone p;
    const QRect canvasBefore = p.at(p.canvas);
    auto* tabs = p.editor->findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    p.child<QPushButton>("sheet:style")->click();
    QApplication::processEvents();
    ASSERT_TRUE(p.sheet->isVisible());
    EXPECT_TRUE(p.editor->isVisible());
    EXPECT_TRUE(p.sheet->isAncestorOf(p.editor));
    const QRect sheet = p.at(p.sheet);
    EXPECT_NEAR(sheet.top(), 0.45 * p.page.height(), 2);
    EXPECT_EQ(sheet.bottom(), p.page.height() - 1);
    EXPECT_EQ(sheet.left(), 0);
    EXPECT_EQ(sheet.width(), p.page.width());
    EXPECT_EQ(p.at(p.canvas), canvasBefore);  // over the canvas, not pushing it
    EXPECT_EQ(tabs->currentIndex(), 1);       // Style
    // the handle closes it; Data opens it on the Data tab
    p.child<QAbstractButton>("plotSheetHandle")->click();
    QApplication::processEvents();
    EXPECT_FALSE(p.sheet->isVisible());
    p.child<QPushButton>("sheet:data")->click();
    QApplication::processEvents();
    EXPECT_TRUE(p.sheet->isVisible());
    EXPECT_EQ(tabs->currentIndex(), 0);
    // Escape closes it too
    QTest::keyClick(QApplication::focusWidget() ? QApplication::focusWidget() : p.sheet, Qt::Key_Escape);
    QApplication::processEvents();
    EXPECT_FALSE(p.sheet->isVisible());
}

// Find… opens the Style tab with the cursor in the search box.
TEST(PlotPhone, FindOpensTheSearch) {
    Phone p;
    p.child<QPushButton>("sheet:find")->click();
    QApplication::processEvents();
    ASSERT_TRUE(p.sheet->isVisible());
    auto* search = p.editor->findChild<QLineEdit*>("plotSearch");
    ASSERT_NE(search, nullptr);
    EXPECT_TRUE(QTest::qWaitFor([&] { return search->hasFocus(); }, 2000));
    EXPECT_EQ(p.editor->findChild<QTabWidget*>()->currentIndex(), 1);
}

// An edit in the sheet is the canvas' new document, as beside it.
TEST(PlotPhone, AnEditInTheSheetReachesTheCanvas) {
    Phone p;
    p.child<QPushButton>("sheet:style")->click();
    QApplication::processEvents();
    auto* title = p.editor->findChild<QLineEdit*>("edit:title");
    ASSERT_NE(title, nullptr);
    title->setFocus();
    QTest::keyClicks(title, "Squares");
    QTest::keyClick(title, Qt::Key_Return);
    EXPECT_NE(p.canvas->figure().indexOf(R"("call":"set_title","args":["Squares"])"), -1) << p.canvas->figure().toStdString();
}

// Back to wide: the editor beside the canvas again, the tools above it, no sheet or sheet bar.
TEST(PlotPhone, WideAgainPutsTheEditorBesideTheCanvas) {
    Phone p;
    p.child<QPushButton>("sheet:style")->click();
    QApplication::processEvents();
    p.page.resize(1000, 650);
    p.page.setNarrow(false);
    QApplication::processEvents();
    auto* tools = p.child<QWidget>("plotTools");
    EXPECT_FALSE(p.sheet->isVisible());
    EXPECT_FALSE(p.child<QWidget>("plotSheetBar")->isVisible());
    EXPECT_TRUE(p.editor->isVisible());
    EXPECT_FALSE(p.sheet->isAncestorOf(p.editor));
    EXPECT_GE(p.at(p.editor).left(), p.at(p.canvas).right());
    EXPECT_LE(p.at(tools).bottom(), p.at(p.canvas).top());
    // and narrow once more: the sheet closed
    p.page.resize(390, 760);
    p.page.setNarrow(true);
    QApplication::processEvents();
    EXPECT_FALSE(p.editor->isVisible());
    EXPECT_FALSE(p.sheet->isVisible());
    EXPECT_GE(p.at(tools).top(), p.at(p.canvas).bottom());
}
