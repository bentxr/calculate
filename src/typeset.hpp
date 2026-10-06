#pragma once

#include "entry.hpp"
#include "presenter.hpp"

#include <QFont>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

class QPainter;

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

// A place for the cursor in the input, where it is drawn: for clicks, the caret and the selection.
struct Mark {
    Position at;
    QRectF caret;  // zero wide, one line of its row's font tall
    QRectF item;   // the item that starts here, ascent to descent; empty at the end of a row
};

struct Box {
    QList<Run> runs;
    QList<QLineF> lines;
    QList<Mark> marks;  // the input's only
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
Box subscript(const Box& base, const Box& index);  // the index comes in its smaller font
// √ over the content, with an optional index (ⁿ√) in its smaller font over the tick.
Box radical(const Box& content, const QFont& font, const Box& index = Box{});
// A large operator such as Σ with its limits centred under and over it.
Box bigOperator(const QString& symbol, const Box& under, const Box& over, const QFont& font);
// Text in several roles, broken between characters into lines no wider than maxWidth.
Box paragraph(const QList<Segment>& segments, const QFont& font, qreal maxWidth);
// Where the box's last line of text ends, on its baseline (the origin for an empty box).
QPointF end(const Box& box);

// Draws the box with its baseline origin at `origin`: text in `ink`, noise digits in `noise`. Runs
// outside `clip` are skipped, so a result of thousands of lines paints quickly.
void paint(QPainter& painter, const Box& box, QPointF origin, const QColor& ink, const QColor& noise, const QRectF& clip);

// The input as the calculator draws it: templates in two dimensions, an empty box as □, every place
// for the cursor marked. `caret` (if any) receives the cursor's rectangle. With `decimalComma`, the point is
// drawn as a comma and the argument separator as a semicolon (the entry keeps the engine's pieces).
Box input(const Entry& entry, const QFont& font, QRectF* caret, bool decimalComma = false);
// The place for the cursor nearest to `point`: first the nearest line, then the nearest place on it.
Position hit(const Box& input, QPointF point);
// What the entry's selection covers in its input box; empty without a selection.
QRectF selectionRect(const Box& input, const Entry& entry);

// The screen's results: trusted|noise digits with any ×10 exponent, and fraction = decimal.
Box value(const view::ValueParts& parts, const QFont& font, qreal maxWidth);
Box exact(const view::FractionParts& parts, const QFont& font, qreal maxWidth);

}  // namespace typeset
