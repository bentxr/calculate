#pragma once

#include <calculate-plot/calculate-plot.hpp>

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QPointF>
#include <QSizeF>
#include <QString>

#include <atomic>

// Draws figure files on its own thread (D9.11). request() and requestText() run on the caller's thread and mark a
// serial the newest of its kind; the jobs run on the worker's and skip a request that a newer one has replaced, so
// the newest wins (a gesture's last step, `commit`, is never skipped).
class PlotWorker : public QObject {
    Q_OBJECT

public:
    void request(int serial) { newest_.store(serial); }
    void requestText(int serial) { newestText_.store(serial); }

public slots:
    // The figure in a widget of `size` logical pixels at `ratio` device pixels each.
    void draw(int serial, const QByteArray& figure, QSizeF size, double ratio);
    // A gesture of the navigation toolbar on `figure`: the figure it leaves, drawn.
    void navigate(int serial, const QByteArray& figure, QSizeF size, double ratio, const calculate_plot::Gesture& gesture, bool commit);
    // The toolbar's text for the pointer at `at`.
    void describe(int serial, const QByteArray& figure, QSizeF size, double ratio, QPointF at);

signals:
    // The image (its device pixel ratio set), or a null image and why the figure cannot be drawn.
    void drawn(int serial, const QImage& image, const QString& error);
    void navigated(int serial, const QByteArray& figure, const QImage& image, const QString& error, bool commit);
    void described(int serial, const QString& text);

private:
    std::atomic<int> newest_{0};
    std::atomic<int> newestText_{0};
};
