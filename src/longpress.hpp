#pragma once

#include <QObject>
#include <QPoint>
#include <QTimer>

class QWidget;

// A press held still for half a second, on a widget that has no gesture of its own for it (a list's rows,
// later the keys). A touch arrives as the same mouse press. The release that ends a long press is
// swallowed, so the press doesn't also count as a click.
class LongPress : public QObject {
    Q_OBJECT

public:
    explicit LongPress(QWidget* widget);  // installs itself on the widget, which owns it

    static constexpr int delay = 500;  // ms

signals:
    void longPressed(QPoint position);  // in the widget's coordinates

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTimer timer_;
    QPoint pressedAt_;
    bool swallowRelease_ = false;
};
