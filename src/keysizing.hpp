#pragma once

#include <QSize>

// The size of one key. The keys are as large as they would be in a window half as wide as the screen
// and as tall as it: that window's room, less what it `reserved` for everything else, shared by a
// `grid` of columns × rows with `spacing` between keys. A key is 0.55 as tall as it is wide (the
// tighter of the two directions decides), and never smaller than `minimum`, what its label needs.
// The result depends only on the screen, never on the window's current size.
QSize keySize(QSize screen, QSize reserved, QSize grid, int spacing, QSize minimum);
