#pragma once

#include "presenter.hpp"

#include <QFrame>

class QLabel;
class QPushButton;
class QScrollArea;
class QToolButton;

// Everything about the error beyond the value on the screen: one row per figure, each with an info sign that
// opens its explanation in a small popup under it. The card floats over the window as a popup, so
// opening it moves nothing, and a click anywhere else closes it.
class DetailsCard : public QFrame {
    Q_OBJECT

public:
    explicit DetailsCard(QWidget* parent = nullptr);

    void setRows(const QList<view::DetailRow>& rows);
    void popUp(QWidget* under);  // just below `under` and as wide as it, inside the window
    void setInspectable(bool on);  // whether "Open in the IEEE 754 tool" is offered (floating results)

signals:
    void inspectRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void explain(const QString& key, QToolButton* info);

    QScrollArea* scroll_ = nullptr;  // the rows scroll when the window has no room for them all
    QWidget* rows_ = nullptr;        // rebuilt for every result
    QFrame* tip_ = nullptr;    // the explanation popup, object "explanation"
    QLabel* tipText_ = nullptr;
    QPushButton* inspect_ = nullptr;
};
