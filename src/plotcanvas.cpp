#include "plotcanvas.hpp"

#include "plotfonts.hpp"
#include "plotworker.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>

PlotCanvas::PlotCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("plotCanvas");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);  // the toolbar's keys
    setMouseTracking(true);           // the pointer's coordinates
    band_ = new QRubberBand(QRubberBand::Rectangle, this);
    band_->setObjectName("plotRubberBand");
    worker_ = new PlotWorker;
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &PlotWorker::drawn, this, &PlotCanvas::receive);
    connect(worker_, &PlotWorker::navigated, this, &PlotCanvas::receiveView);
    connect(worker_, &PlotWorker::described, this, [this](int serial, const QString& text) {
        if (serial != textAsked_ || text == coordinates_) return;
        coordinates_ = text;
        emit coordinatesChanged(text);
    });
    thread_.start();
}

PlotCanvas::~PlotCanvas() {
    worker_->request(-1);  // whatever is queued is skipped
    worker_->requestText(-1);
    thread_.quit();
    thread_.wait();
}

void PlotCanvas::setFigure(const QByteArray& figure) {
    figure_ = figure;
    views_ = {figure};
    view_ = 0;
    emit viewsChanged();
    ask();
}

QByteArray PlotCanvas::pngBytes() const {
    ensurePlotFonts();
    std::string error;
    return QByteArray::fromStdString(
        calculate_plot::drawPng(std::string_view(figure_.constData(), static_cast<std::size_t>(figure_.size())), std::string(), &error));
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

void PlotCanvas::setError(const QString& error) {
    if (error == error_) return;
    error_ = error;
    emit errorChanged(error_);
}

void PlotCanvas::receive(int serial, const QImage& image, const QString& error) {
    if (serial != asked_) return;  // an older drawing: a newer one is on its way
    shown_ = serial;
    if (!image.isNull()) image_ = image;
    setError(error);
    update();
}

void PlotCanvas::receiveView(int serial, const QByteArray& figure, const QImage& image, const QString& error, bool commit) {
    if (commit) {
        if (error.isEmpty() && figure != views_.value(view_)) push(figure);  // (a gesture that did nothing leaves no view)
        else if (!error.isEmpty()) figure_ = views_.value(view_);
    } else if (error.isEmpty() && serial == asked_) {
        figure_ = figure;  // the view while dragging
    }
    if (serial != asked_) return;
    shown_ = serial;
    if (!image.isNull()) image_ = image;
    setError(error);
    update();
}

void PlotCanvas::push(const QByteArray& figure) {  // NavigationToolbar2.push_current: the views ahead go
    while (views_.size() > view_ + 1) views_.removeLast();
    views_.append(figure);
    view_ = int(views_.size()) - 1;
    figure_ = figure;
    emit viewsChanged();
}

void PlotCanvas::showView(int index) {
    view_ = index;
    figure_ = views_[index];
    emit viewsChanged();
    ask();
}

void PlotCanvas::home() {  // the stack's home: the first view pushed again
    if (views_.isEmpty()) return;
    push(views_.first());
    ask();
}

void PlotCanvas::back() {
    if (canGoBack()) showView(view_ - 1);
}

void PlotCanvas::forward() {
    if (canGoForward()) showView(view_ + 1);
}

void PlotCanvas::setTool(Tool tool) {
    tool_ = tool == tool_ ? Tool::None : tool;
    emit toolChanged(tool_);
}

std::string PlotCanvas::key(Qt::KeyboardModifiers modifiers) const {
    if (heldKey_ == 'x') return "x";
    if (heldKey_ == 'y') return "y";
    if (modifiers & Qt::ControlModifier) return "control";
    if (modifiers & Qt::ShiftModifier) return "shift";
    return std::string();
}

void PlotCanvas::askGesture(QPointF to, Qt::KeyboardModifiers modifiers, bool commit) {
    calculate_plot::Gesture g;
    g.tool = tool_ == Tool::Zoom ? calculate_plot::Gesture::Tool::Zoom : calculate_plot::Gesture::Tool::Pan;
    g.button = pressButton_;
    g.key = key(modifiers);
    g.fromX = pressAt_.x(), g.fromY = pressAt_.y(), g.toX = to.x(), g.toY = to.y();
    const int serial = ++asked_;
    worker_->request(serial);
    const QByteArray figure = pressFigure_;  // always from the view at the press (start_pan's frozen state)
    const QSizeF size = this->size();
    const double ratio = devicePixelRatioF();
    PlotWorker* worker = worker_;
    QMetaObject::invokeMethod(worker_, [worker, serial, figure, size, ratio, g, commit] { worker->navigate(serial, figure, size, ratio, g, commit); });
}

void PlotCanvas::askCoordinates(QPointF at) {
    if (figure_.isEmpty()) return;
    const int serial = ++textAsked_;
    worker_->requestText(serial);
    const QByteArray figure = figure_;
    const QSizeF size = this->size();
    const double ratio = devicePixelRatioF();
    PlotWorker* worker = worker_;
    QMetaObject::invokeMethod(worker_, [worker, serial, figure, size, ratio, at] { worker->describe(serial, figure, size, ratio, at); });
}

void PlotCanvas::mousePressEvent(QMouseEvent* event) {
    if (tool_ == Tool::None || (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)) return QWidget::mousePressEvent(event);
    pressed_ = true;
    pressAt_ = event->position();
    pressButton_ = event->button() == Qt::LeftButton ? 1 : 3;
    pressFigure_ = views_.value(view_);
    if (tool_ == Tool::Zoom) band_->setGeometry(QRect(pressAt_.toPoint(), QSize()));
}

void PlotCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!pressed_) return askCoordinates(event->position());
    if (tool_ == Tool::Pan) return askGesture(event->position(), event->modifiers(), false);
    band_->setGeometry(QRectF(pressAt_, event->position()).normalized().toRect());
    band_->show();
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (!pressed_) return QWidget::mouseReleaseEvent(event);
    pressed_ = false;
    band_->hide();
    askGesture(event->position(), event->modifiers(), true);
}

void PlotCanvas::keyPressEvent(QKeyEvent* event) {
    const bool control = event->modifiers() & Qt::ControlModifier;
    switch (event->key()) {
    case Qt::Key_X: heldKey_ = 'x'; break;
    case Qt::Key_Y: heldKey_ = 'y'; break;
    case Qt::Key_P: setTool(Tool::Pan); break;
    case Qt::Key_O: setTool(Tool::Zoom); break;
    case Qt::Key_H:
    case Qt::Key_R:
    case Qt::Key_Home: home(); break;
    case Qt::Key_C:
    case Qt::Key_Left:
    case Qt::Key_Backspace: back(); break;
    case Qt::Key_V:
    case Qt::Key_Right: forward(); break;
    case Qt::Key_S:
        if (control) {
            emit saveRequested();
            break;
        }
        return QWidget::keyPressEvent(event);
    default: return QWidget::keyPressEvent(event);
    }
}

void PlotCanvas::keyReleaseEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_X && heldKey_ == 'x') || (event->key() == Qt::Key_Y && heldKey_ == 'y')) heldKey_ = 0;
    QWidget::keyReleaseEvent(event);
}

void PlotCanvas::leaveEvent(QEvent*) {
    ++textAsked_;  // an answer on its way is dropped
    if (coordinates_.isEmpty()) return;
    coordinates_.clear();
    emit coordinatesChanged(coordinates_);
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
