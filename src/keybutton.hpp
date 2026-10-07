#pragma once

#include <QPushButton>
#include <QTimer>

// A calculator key. A key with more faces also offers them on a long press (touch) or a right click
// (mouse), and then doesn't click; a small mark in its corner says it has them.
class KeyButton : public QPushButton {
    Q_OBJECT

public:
    explicit KeyButton(QWidget* parent = nullptr);
    void setHasMore(bool more);
    bool hasMore() const { return hasMore_; }
    static constexpr int longPressMs = 500;

signals:
    void moreRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QTimer hold_;
    bool hasMore_ = false;
    bool offered_ = false;  // the long press fired: the release must not click
};
