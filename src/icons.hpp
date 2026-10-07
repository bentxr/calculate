#pragma once

#include <QColor>
#include <QIcon>

// Icons drawn as line art in one colour, so they look the same with every font and in the browser (the project
// uses no emoji).
namespace icons {

enum class Kind { Settings, Keyboard, Search };

// A square icon of `side` logical pixels in `ink`, transparent elsewhere, sharp at the screen's pixel ratio.
QIcon drawn(Kind kind, const QColor& ink, int side);

}  // namespace icons
