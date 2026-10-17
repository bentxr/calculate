#include "plotcanvas.hpp"
#include "plotpage.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QTest>
#include <QToolButton>

#include "printers.hpp"

#include <gtest/gtest.h>

// The plot page's navigation as matplotlib's toolbar: Home, Back, Forward, Pan, Zoom and Save, the keys it knows,
// and the pointer's coordinates. The figures it leaves are the library's own (calculate_plot::navigate).
namespace {

const QByteArray kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
    {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]]}]})";

struct Page {
    PlotPage page;
    PlotCanvas* canvas = nullptr;
    Page() {
        page.resize(900, 600);
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
    QToolButton* button(const char* name) { return page.findChild<QToolButton*>(name); }
    calculate_plot::Canvas canvasSize() const {
        return calculate_plot::Canvas{double(canvas->width()), double(canvas->height()), canvas->devicePixelRatioF()};
    }
    // the library's figure after a gesture on the canvas
    QByteArray expected(const QByteArray& figure, calculate_plot::Gesture g) const {
        std::string error;
        const std::string out = calculate_plot::navigate(std::string_view(figure.constData(), std::size_t(figure.size())), canvasSize(), g,
                                                         std::string(CALCULATE_PLOT_FONTS), &error);
        EXPECT_EQ(error, "");
        return QByteArray::fromStdString(out);
    }
};

void mouse(QWidget* w, QEvent::Type type, QPointF at, Qt::MouseButton button, Qt::MouseButtons buttons,
           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, at, w->mapToGlobal(at), button, buttons, modifiers);
    QApplication::sendEvent(w, &event);
}

// A drag from `from` to `to` with a button held, through a point between.
void drag(QWidget* w, QPointF from, QPointF to, Qt::MouseButton button, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    mouse(w, QEvent::MouseButtonPress, from, button, button, modifiers);
    mouse(w, QEvent::MouseMove, (from + to) / 2, Qt::NoButton, button, modifiers);
    mouse(w, QEvent::MouseMove, to, Qt::NoButton, button, modifiers);
    mouse(w, QEvent::MouseButtonRelease, to, button, Qt::NoButton, modifiers);
}

calculate_plot::Gesture gesture(calculate_plot::Gesture::Tool tool, QPointF from, QPointF to, int button = 1, const char* key = "") {
    calculate_plot::Gesture g;
    g.tool = tool;
    g.button = button;
    g.key = key;
    g.fromX = from.x(), g.fromY = from.y(), g.toX = to.x(), g.toY = to.y();
    return g;
}

}  // namespace

TEST(PlotNavigation, TheToolbarHasMatplotlibsTools) {
    Page p;
    for (const char* name : {"plotHome", "plotBack", "plotForward", "plotPan", "plotZoom", "plotSave"}) {
        QToolButton* b = p.button(name);
        ASSERT_NE(b, nullptr) << name;
        EXPECT_TRUE(b->isVisible()) << name;
        EXPECT_FALSE(b->icon().isNull()) << name;
        EXPECT_FALSE(b->toolTip().isEmpty()) << name;
        EXPECT_FALSE(b->accessibleName().isEmpty()) << name;
    }
    EXPECT_TRUE(p.button("plotPan")->isCheckable());
    EXPECT_TRUE(p.button("plotZoom")->isCheckable());
    EXPECT_FALSE(p.button("plotBack")->isEnabled());  // nothing to go back to yet
    EXPECT_FALSE(p.button("plotForward")->isEnabled());
    // pan and zoom exclude each other, and a second press leaves both off
    p.button("plotPan")->click();
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::Pan);
    p.button("plotZoom")->click();
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::Zoom);
    EXPECT_FALSE(p.button("plotPan")->isChecked());
    p.button("plotZoom")->click();
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::None);
}

// Pan: the left button moves the view, the right one zooms it about the press; the figure is the library's.
TEST(PlotNavigation, PanMovesTheViewAsTheLibraryDoes) {
    Page p;
    p.button("plotPan")->click();
    const QPointF from(300, 250), to(340, 230);
    drag(p.canvas, from, to, Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy(); }, 10000));
    EXPECT_EQ(p.canvas->figure(), p.expected(kFigure, gesture(calculate_plot::Gesture::Tool::Pan, from, to)));
    EXPECT_TRUE(p.button("plotBack")->isEnabled());
    const QByteArray panned = p.canvas->figure();
    drag(p.canvas, from, QPointF(320, 270), Qt::RightButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != panned; }, 10000));
    EXPECT_EQ(p.canvas->figure(), p.expected(panned, gesture(calculate_plot::Gesture::Tool::Pan, from, QPointF(320, 270), 3)));
}

// Holding x keeps a pan to the horizontal; control keeps it on the diagonal.
TEST(PlotNavigation, KeysHeldDuringAPanAreMatplotlibs) {
    Page p;
    p.button("plotPan")->click();
    const QPointF from(300, 250), to(340, 230);
    QTest::keyPress(p.canvas, Qt::Key_X);
    drag(p.canvas, from, to, Qt::LeftButton);
    QTest::keyRelease(p.canvas, Qt::Key_X);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    EXPECT_EQ(p.canvas->figure(), p.expected(kFigure, gesture(calculate_plot::Gesture::Tool::Pan, from, to, 1, "x")));
    p.button("plotHome")->click();
    drag(p.canvas, from, to, Qt::LeftButton, Qt::ControlModifier);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    EXPECT_EQ(p.canvas->figure(), p.expected(kFigure, gesture(calculate_plot::Gesture::Tool::Pan, from, to, 1, "control")));
}

// Zoom: a rectangle shows while dragging; the release zooms in (left button) as the library does.
TEST(PlotNavigation, ZoomToARectangle) {
    Page p;
    p.button("plotZoom")->click();
    const QPointF from(250, 200), to(380, 320);
    mouse(p.canvas, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    mouse(p.canvas, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    QWidget* band = p.canvas->findChild<QWidget*>("plotRubberBand");
    ASSERT_NE(band, nullptr);
    EXPECT_TRUE(band->isVisible());
    EXPECT_EQ(band->geometry(), QRectF(from, to).normalized().toRect());
    mouse(p.canvas, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    EXPECT_FALSE(band->isVisible());
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    EXPECT_EQ(p.canvas->figure(), p.expected(kFigure, gesture(calculate_plot::Gesture::Tool::Zoom, from, to)));
    // less than 5 pixels does nothing
    const QByteArray zoomed = p.canvas->figure();
    drag(p.canvas, from, from + QPointF(3, 30), Qt::LeftButton);
    QTest::qWait(300);
    EXPECT_EQ(p.canvas->figure(), zoomed);
}

// Back, Forward and Home walk the views the gestures left; a new gesture after Back drops the views ahead.
TEST(PlotNavigation, BackForwardAndHomeWalkTheViews) {
    Page p;
    p.button("plotPan")->click();
    drag(p.canvas, QPointF(300, 250), QPointF(340, 230), Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    const QByteArray first = p.canvas->figure();
    drag(p.canvas, QPointF(300, 250), QPointF(260, 280), Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != first; }, 10000));
    const QByteArray second = p.canvas->figure();
    p.button("plotBack")->click();
    EXPECT_EQ(p.canvas->figure(), first);
    EXPECT_TRUE(p.button("plotForward")->isEnabled());
    p.button("plotForward")->click();
    EXPECT_EQ(p.canvas->figure(), second);
    EXPECT_FALSE(p.button("plotForward")->isEnabled());
    p.button("plotHome")->click();
    EXPECT_EQ(p.canvas->figure(), kFigure);
    EXPECT_TRUE(p.button("plotBack")->isEnabled());  // Home is a view too: Back returns from it
    p.button("plotBack")->click();
    EXPECT_EQ(p.canvas->figure(), second);
    p.button("plotBack")->click();
    p.button("plotBack")->click();
    EXPECT_EQ(p.canvas->figure(), kFigure);
    EXPECT_FALSE(p.button("plotBack")->isEnabled());
    drag(p.canvas, QPointF(300, 250), QPointF(310, 300), Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    EXPECT_FALSE(p.button("plotForward")->isEnabled());
    ASSERT_TRUE(p.settle());
}

// matplotlib's keys: p pan, o zoom, h / r / Home home, c / Left / Backspace back, v / Right forward.
TEST(PlotNavigation, TheToolbarsKeys) {
    Page p;
    QTest::keyClick(p.canvas, Qt::Key_P);
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::Pan);
    EXPECT_TRUE(p.button("plotPan")->isChecked());
    drag(p.canvas, QPointF(300, 250), QPointF(340, 230), Qt::LeftButton);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !p.canvas->busy() && p.canvas->figure() != kFigure; }, 10000));
    const QByteArray panned = p.canvas->figure();
    for (Qt::Key back : {Qt::Key_C, Qt::Key_Left, Qt::Key_Backspace}) {
        QTest::keyClick(p.canvas, back);
        EXPECT_EQ(p.canvas->figure(), kFigure) << back;
        QTest::keyClick(p.canvas, back == Qt::Key_Left ? Qt::Key_Right : Qt::Key_V);
        EXPECT_EQ(p.canvas->figure(), panned) << back;
    }
    for (Qt::Key home : {Qt::Key_H, Qt::Key_R, Qt::Key_Home}) {
        QTest::keyClick(p.canvas, Qt::Key_V);  // (to the newest view)
        QTest::keyClick(p.canvas, home);
        EXPECT_EQ(p.canvas->figure(), kFigure) << home;
        QTest::keyClick(p.canvas, Qt::Key_C);
    }
    QTest::keyClick(p.canvas, Qt::Key_O);
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::Zoom);
    EXPECT_TRUE(p.button("plotZoom")->isChecked());
    QTest::keyClick(p.canvas, Qt::Key_O);
    EXPECT_EQ(p.canvas->tool(), PlotCanvas::Tool::None);
}

// Hovering shows the coordinates as matplotlib's toolbar writes them; outside the Axes nothing.
TEST(PlotNavigation, ThePointersCoordinates) {
    Page p;
    auto* line = p.page.findChild<QLabel*>("plotCoordinates");
    ASSERT_NE(line, nullptr);
    const QPointF at(300, 250);
    mouse(p.canvas, QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
    std::string error;
    const auto texts = calculate_plot::pointerTexts(std::string_view(kFigure.constData(), std::size_t(kFigure.size())), p.canvasSize(),
                                                    {{at.x(), at.y()}, {3.0, 3.0}}, std::string(CALCULATE_PLOT_FONTS), &error);
    ASSERT_EQ(texts.size(), 2u);
    ASSERT_FALSE(texts[0].empty());
    EXPECT_TRUE(QTest::qWaitFor([&] { return line->text() == QString::fromStdString(texts[0]); }, 10000)) << line->text().toStdString();
    mouse(p.canvas, QEvent::MouseMove, QPointF(3, 3), Qt::NoButton, Qt::NoButton);
    EXPECT_TRUE(QTest::qWaitFor([&] { return line->text().isEmpty(); }, 10000));
    EXPECT_EQ(texts[1], "");
}

// Save gives the figure as matplotlib's savefig writes a PNG.
TEST(PlotNavigation, SaveWritesThePng) {
    Page p;
    std::string error;
    const std::string png = calculate_plot::drawPng(std::string_view(kFigure.constData(), std::size_t(kFigure.size())),
                                                    std::string(CALCULATE_PLOT_FONTS), &error);
    ASSERT_EQ(error, "");
    EXPECT_EQ(p.canvas->pngBytes(), QByteArray::fromStdString(png));
}
