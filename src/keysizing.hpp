#pragma once

#include <QFont>
#include <QSize>
#include <QString>

// The size of one key. The keys are as large as they would be in a window half as wide as the screen
// and as tall as it: that window's room, less what it `reserved` for everything else, shared by a
// `grid` of columns × rows with `spacing` between keys. A key is 0.55 as tall as it is wide (the
// tighter of the two directions decides), and never smaller than `minimum`, what its label needs.
// The result depends only on the screen, never on the window's current size.
QSize keySize(QSize screen, QSize reserved, QSize grid, int spacing, QSize minimum);

// How the window arranges the keys: side by side, or (on a portrait screen, such as a phone's) the phone
// arrangement: Common above the main pad, the rest in a drawer.
enum class KeysLayout { Wide, Narrow };
KeysLayout keysLayout(QSize screen);  // Narrow when the screen is narrower than it is tall

// The smallest key height a finger hits reliably, in logical pixels (the platforms' 44 pt).
inline constexpr int touchTarget = 44;

// The size of one key in the phone arrangement: the screen's width, less what the window `reservedWidth` for
// everything else, shared by `columns` keys with `spacing` between them, never narrower than `minimum` nor wider
// than twice it; 0.55 as tall as wide, never below `minimum`'s height (the caller passes at least touchTarget).
QSize phoneKeySize(QSize screen, int reservedWidth, int columns, int spacing, QSize minimum);

// The font for a key's label: `font` itself when the label fits in `width`, or the largest smaller one
// that makes it fit, never under `smallest` of its size (an even longer label is then cut at the edge).
QFont fittedFont(const QFont& font, const QString& label, int width, qreal smallest = 0.6);
