#include "plotcanvas.hpp"

#include "plotworker.hpp"

#include <QPainter>

PlotCanvas::PlotCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("plotCanvas");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    worker_ = new PlotWorker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &PlotWorker::drawn, this, &PlotCanvas::receive);
    thread_.start();
}

PlotCanvas::~PlotCanvas() {
    worker_->request(-1);  // whatever is queued is skipped
    thread_.quit();
    thread_.wait();
}

void PlotCanvas::setFigure(const QByteArray& figure) {
    figure_ = figure;
    ask();
}

void PlotCanvas::ask() {
    if (!isVisible()) {  // showEvent asks
        pending_ = true;
        return;
    }
    pending_ = false;
    if (figure_.isEmpty() || width() <= 0 || height() <= 0) return;
    const int serial = ++asked_;
    worker_->request(serial);
    const QByteArray figure = figure_;
    const QSizeF size = this->size();
    const double ratio = devicePixelRatioF();
    PlotWorker* worker = worker_;
    QMetaObject::invokeMethod(worker_, [worker, serial, figure, size, ratio] { worker->draw(serial, figure, size, ratio); });
}

void PlotCanvas::receive(int serial, const QImage& image, const QString& error) {
    if (serial != asked_) return;  // an older drawing: a newer one is on its way
    shown_ = serial;
    if (!image.isNull()) image_ = image;
    if (error != error_) {
        error_ = error;
        emit errorChanged(error_);
    }
    update();
}

void PlotCanvas::resizeEvent(QResizeEvent*) { ask(); }

void PlotCanvas::showEvent(QShowEvent*) {
    if (pending_) ask();
}

void PlotCanvas::paintEvent(QPaintEvent*) {
    // The image at its own size (matplotlib's canvas does not stretch a buffer of another size), the rest in the
    // figure's colour of the last image's corner.
    QPainter painter(this);
    if (image_.isNull()) return;
    painter.fillRect(rect(), image_.pixelColor(0, 0));
    painter.drawImage(QPoint(0, 0), image_);
}
