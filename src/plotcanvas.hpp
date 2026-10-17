#pragma once

#include <calculate-plot/calculate-plot.hpp>

#include <QImage>
#include <QList>
#include <QThread>
#include <QWidget>

class PlotWorker;
class QRubberBand;

// The figure, drawn by the engine at the widget's size and the screen's device pixel ratio (D9.3), shown as is. Each
// change of figure or size asks the worker for a new image; the last one stays until it arrives.
//
// Navigation is matplotlib's toolbar (D9.4): Pan (left button moves, right zooms about the press) and Zoom (to a
// rectangle; in with the left button, out with the right), the keys it knows, and the views they leave kept as
// figure files: Home pushes the first view again, Back and Forward walk them.
class PlotCanvas : public QWidget {
    Q_OBJECT

public:
    enum class Tool { None, Pan, Zoom };

    explicit PlotCanvas(QWidget* parent = nullptr);
    ~PlotCanvas() override;

    // A new document: its views start with it.
    void setFigure(const QByteArray& figure);
    QByteArray figure() const { return figure_; }
    QImage image() const { return image_; }  // the last image drawn
    QString error() const { return error_; }  // why the figure cannot be drawn, or empty
    bool busy() const { return asked_ != shown_; }  // an image is on its way
    QString coordinates() const { return coordinates_; }  // the toolbar's text for the pointer
    QByteArray pngBytes() const;                          // the figure as savefig writes a PNG

    Tool tool() const { return tool_; }
    void setTool(Tool tool);  // the tool, or none when it is the one in use (as the toolbar's buttons)
    bool canGoBack() const { return view_ > 0; }
    bool canGoForward() const { return view_ + 1 < views_.size(); }

public slots:
    void home();
    void back();
    void forward();

signals:
    void errorChanged(const QString& error);
    void toolChanged(Tool tool);
    void viewsChanged();
    void coordinatesChanged(const QString& text);
    void saveRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void ask();
    void receive(int serial, const QImage& image, const QString& error);
    void receiveView(int serial, const QByteArray& figure, const QImage& image, const QString& error, bool commit);
    void showView(int index);
    void push(const QByteArray& figure);
    void askGesture(QPointF to, Qt::KeyboardModifiers modifiers, bool commit);
    void askCoordinates(QPointF at);
    void setError(const QString& error);
    std::string key(Qt::KeyboardModifiers modifiers) const;  // matplotlib's event.key

    QThread thread_;
    PlotWorker* worker_ = nullptr;
    QByteArray figure_;
    QImage image_;
    QString error_;
    int asked_ = 0;
    int shown_ = 0;
    bool pending_ = false;  // a change came while hidden
    QList<QByteArray> views_;
    int view_ = 0;
    Tool tool_ = Tool::None;
    bool pressed_ = false;  // a gesture under way
    QPointF pressAt_;
    int pressButton_ = 1;
    QByteArray pressFigure_;
    char heldKey_ = 0;  // x or y, held
    QRubberBand* band_ = nullptr;
    int textAsked_ = 0;
    QString coordinates_;
};
