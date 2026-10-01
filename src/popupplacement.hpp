#pragma once

#include <QRect>

class QWidget;

// Where a popup of `size` goes so that it stays inside `bounds` (all in global coordinates): just under
// `anchor`, or just over it when only that side has room; when neither has, on the roomier side, cut
// to fit. It never covers the anchor. Left edges line up, moved left as far as it must, and a popup
// wider than `bounds` is cut to its width.
QRect placed(QSize size, QRect anchor, QRect bounds);

// `rect` moved inside `bounds`, and cut to its size if it is larger.
QRect keptInside(QRect rect, QRect bounds);

// Where `widget` is, in global coordinates: what a popup is anchored to.
QRect globalGeometry(const QWidget* widget);

// The global geometry of the window `widget` belongs to: its outermost window, past any popup or dialog.
QRect popupBounds(const QWidget* widget);
