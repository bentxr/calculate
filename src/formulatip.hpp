#pragma once

#include "typeset.hpp"

#include <QWidget>

// A tooltip that shows how a statistic is computed: its formula, typeset like the screen, and one
// line of words under it.
class FormulaTip : public QWidget {
    Q_OBJECT

public:
    explicit FormulaTip(QWidget* parent = nullptr);

    void showFor(const QString& function, const QPoint& at);  // at: a global position, the pointer's
    QString function() const { return function_; }
    QString algorithm() const;
    qreal formulaHeight() const { return formula_.ascent + formula_.descent; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRect textRect() const;  // where the line of words goes, under the formula

    QString function_;
    typeset::Box formula_;
};
