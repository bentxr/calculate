#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSizeF>
#include <QString>

#include <atomic>

// Draws figure files on its own thread (D9.11). request() runs on the caller's thread and marks a serial the newest;
// draw() runs on the worker's and skips a request that a newer one has replaced, so the newest wins.
class PlotWorker : public QObject {
    Q_OBJECT

public:
    void request(int serial) { newest_.store(serial); }

public slots:
    // The figure in a widget of `size` logical pixels at `ratio` device pixels each.
    void draw(int serial, const QByteArray& figure, QSizeF size, double ratio);

signals:
    // The image (its device pixel ratio set), or a null image and why the figure cannot be drawn.
    void drawn(int serial, const QImage& image, const QString& error);

private:
    std::atomic<int> newest_{0};
};
