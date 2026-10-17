#include "plotcanvas.hpp"
#include "plotworker.hpp"

#include "mainwindow.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QApplication>
#include <QListWidget>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include "printers.hpp"

#include <gtest/gtest.h>

namespace {

const QByteArray kFigure = R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]},
    {"on": "ax", "call": "plot", "args": [[0, 1, 2, 3], [0, 1, 4, 9]], "kwargs": {"label": "squares"}},
    {"on": "ax", "call": "set_title", "args": ["Squares"]}, {"on": "ax", "call": "legend"}]})";

// The engine's own buffer for a canvas, as a QImage to compare with.
QImage engineImage(const QByteArray& figure, QSize size, double ratio) {
    std::string error;
    const calculate_plot::Image image = calculate_plot::drawCanvas(
        std::string_view(figure.constData(), static_cast<std::size_t>(figure.size())),
        calculate_plot::Canvas{double(size.width()), double(size.height()), ratio}, std::string(CALCULATE_PLOT_FONTS), &error);
    EXPECT_EQ(error, "");
    QImage out(image.width, image.height, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height; ++y)
        std::copy_n(image.rgba.data() + std::size_t(y) * std::size_t(image.width) * 4, std::size_t(image.width) * 4, out.scanLine(y));
    return out;
}

bool drawnAt(PlotCanvas& canvas, QSize size) {
    return QTest::qWaitFor([&] { return canvas.image().size() == size * canvas.devicePixelRatioF() && !canvas.busy(); }, 10000);
}

}  // namespace

// The canvas shows the engine's buffer for its own size, pixel for pixel.
TEST(PlotCanvas, ShowsTheEnginesBufferAtItsSize) {
    PlotCanvas canvas;
    canvas.resize(400, 300);
    canvas.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&canvas));
    canvas.setFigure(kFigure);
    ASSERT_TRUE(drawnAt(canvas, QSize(400, 300)));
    EXPECT_EQ(canvas.error(), "");
    const QImage want = engineImage(kFigure, QSize(400, 300), canvas.devicePixelRatioF());
    EXPECT_EQ(canvas.image().convertToFormat(QImage::Format_RGBA8888), want);
    // and that is what it paints
    EXPECT_EQ(canvas.grab().toImage().convertToFormat(QImage::Format_RGBA8888), want);
}

// A new size draws the figure again at that size; until it arrives the last image stays.
TEST(PlotCanvas, FollowsItsSizeAndKeepsTheLastImageMeanwhile) {
    PlotCanvas canvas;
    canvas.resize(400, 300);
    canvas.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&canvas));
    canvas.setFigure(kFigure);
    ASSERT_TRUE(drawnAt(canvas, QSize(400, 300)));
    canvas.resize(500, 250);
    EXPECT_EQ(canvas.image().size(), QSize(400, 300) * canvas.devicePixelRatioF());  // nothing blank in between
    ASSERT_TRUE(drawnAt(canvas, QSize(500, 250)));
    EXPECT_EQ(canvas.image().convertToFormat(QImage::Format_RGBA8888), engineImage(kFigure, QSize(500, 250), canvas.devicePixelRatioF()));
}

// A figure the engine refuses says why, and the last image stays.
TEST(PlotCanvas, AFigureTheEngineRefusesSaysWhy) {
    PlotCanvas canvas;
    canvas.resize(300, 200);
    canvas.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&canvas));
    canvas.setFigure(kFigure);
    ASSERT_TRUE(drawnAt(canvas, QSize(300, 200)));
    const QImage before = canvas.image();
    QSignalSpy changed(&canvas, &PlotCanvas::errorChanged);
    canvas.setFigure(R"({"version": 1, "calls": [{"call": "bogus"}]})");
    ASSERT_TRUE(QTest::qWaitFor([&] { return !canvas.error().isEmpty(); }, 10000));
    EXPECT_EQ(canvas.error(), "unsupported call: plt.bogus");
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(canvas.image(), before);
    canvas.setFigure(kFigure);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canvas.error().isEmpty(); }, 10000));
}

// The worker draws on its own thread; of requests that queue up, only the newest is drawn.
TEST(PlotWorker, DrawsOnlyTheNewestRequest) {
    QThread thread;
    auto* worker = new PlotWorker;
    worker->moveToThread(&thread);
    QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
    thread.start();
    QList<int> drawn;
    QObject::connect(worker, &PlotWorker::drawn, qApp, [&](int serial, const QImage&, const QString&) { drawn << serial; });
    for (int serial = 1; serial <= 5; ++serial) {
        worker->request(serial);
        QMetaObject::invokeMethod(worker, [=] { worker->draw(serial, kFigure, QSizeF(200 + serial, 150), 1.0); });
    }
    ASSERT_TRUE(QTest::qWaitFor([&] { return !drawn.isEmpty() && drawn.last() == 5; }, 10000));
    QTest::qWait(50);
    EXPECT_EQ(drawn.last(), 5);
    EXPECT_LT(drawn.size(), 5);  // the stale ones were skipped
    thread.quit();
    thread.wait();
}

// In the window, the Plot page's canvas takes the room beside the editor and shows a figure.
TEST(PlotCanvas, IsThePlotPagesCanvas) {
    MainWindow window;
    window.resize(1280, 800);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    window.layOutKeys(QSize(1280, 800));
    window.findChild<QListWidget*>("modes")->setCurrentRow(4);
    QTest::qWait(50);  // (the page's scroll area lays it out)
    auto* canvas = window.findChild<PlotCanvas*>("plotCanvas");
    ASSERT_NE(canvas, nullptr);
    EXPECT_TRUE(canvas->isVisible());
    EXPECT_GT(canvas->width(), 600);
    EXPECT_GT(canvas->height(), 600);
    ASSERT_TRUE(QTest::qWaitFor([&] { return canvas->image().size() == canvas->size() * canvas->devicePixelRatioF(); }, 10000));
    EXPECT_EQ(canvas->error(), "");
}

// Nothing is drawn while the canvas is not on the screen (the calculator's modes pay nothing for Plot); showing it draws.
TEST(PlotCanvas, DrawsOnlyWhenShown) {
    PlotCanvas canvas;
    canvas.resize(300, 200);
    canvas.setFigure(kFigure);
    QTest::qWait(300);
    EXPECT_TRUE(canvas.image().isNull());
    EXPECT_FALSE(canvas.busy());
    canvas.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&canvas));
    EXPECT_TRUE(drawnAt(canvas, QSize(300, 200)));
    // a figure changed while hidden is drawn when shown again
    const QImage first = canvas.image();
    canvas.hide();
    canvas.setFigure(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}]})");
    QTest::qWait(200);
    EXPECT_EQ(canvas.image(), first);
    canvas.show();
    EXPECT_TRUE(QTest::qWaitFor([&] { return canvas.image() != first; }, 10000));
}
