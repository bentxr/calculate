#pragma once

#include "presenter.hpp"

#include <QFrame>

// Everything about the error beyond the value on the screen: one row per figure, each with an ⓘ that
// opens its explanation in place. It floats over the window as a popup, so opening it moves nothing,
// and a click anywhere else closes it.
class DetailsCard : public QFrame {
    Q_OBJECT

public:
    explicit DetailsCard(QWidget* parent = nullptr);

    void setRows(const QList<view::DetailRow>& rows);
    void popUp(QWidget* under);  // just below `under`, as wide as it
};
