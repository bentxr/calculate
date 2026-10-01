#pragma once

#include "presenter.hpp"

#include <QFont>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QString>

// A small layout engine for the calculator's screen: it places text and lines in two dimensions
// (fractions, raised exponents, recurring bars, long values wrapped to the screen). Coordinates are
// relative to the box's baseline at y = 0, with y growing downwards as in Qt.
namespace typeset {

enum class Role { Plain, Noise };

struct Run {
    QString text;
    QPointF origin;  // the left end of the text's baseline
    QFont font;
    Role role = Role::Plain;
};

struct Box {
    QList<Run> runs;
    QList<QLineF> lines;
    qreal width = 0;
    qreal ascent = 0;   // extent above the baseline
    qreal descent = 0;  // extent below it
};

struct Segment {
    QString text;
    Role role = Role::Plain;
};

Box text(const QString& s, const QFont& font, Role role = Role::Plain);
Box row(const QList<Box>& boxes);  // side by side on one baseline
Box fraction(const Box& numerator, const Box& denominator, const QFont& font);
Box superscript(const Box& base, const Box& exponent);  // the exponent comes in its smaller font
Box overline(const Box& box);
// Text in several roles, broken between characters into lines no wider than maxWidth.
Box paragraph(const QList<Segment>& segments, const QFont& font, qreal maxWidth);
// Where the box's last line of text ends, on its baseline (the origin for an empty box).
QPointF end(const Box& box);

// The screen's results: trusted|noise digits with any ×10 exponent, and fraction = decimal.
Box value(const view::ValueParts& parts, const QFont& font, qreal maxWidth);
Box exact(const view::FractionParts& parts, const QFont& font, qreal maxWidth);

}  // namespace typeset
