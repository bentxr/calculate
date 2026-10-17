#pragma once

#include <QImage>
#include <QThread>
#include <QWidget>

class PlotWorker;

// The figure, drawn by the engine at the widget's size and the screen's device pixel ratio (D9.3), shown as is. Each
// change of figure or size asks the worker for a new image; the last one stays until it arrives.
class PlotCanvas : public QWidget {
    Q_OBJECT

public:
    explicit PlotCanvas(QWidget* parent = nullptr);
    ~PlotCanvas() override;

    void setFigure(const QByteArray& figure);
    QByteArray figure() const { return figure_; }
    QImage image() const { return image_; }  // the last image drawn
    QString error() const { return error_; }  // why the figure cannot be drawn, or empty
    bool busy() const { return asked_ != shown_; }  // an image is on its way

signals:
    void errorChanged(const QString& error);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void ask();
    void receive(int serial, const QImage& image, const QString& error);

    QThread thread_;
    PlotWorker* worker_ = nullptr;
    QByteArray figure_;
    QImage image_;
    QString error_;
    int asked_ = 0;
    int shown_ = 0;
    bool pending_ = false;  // a change came while hidden
};
