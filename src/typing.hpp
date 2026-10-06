#pragma once

#include "entry.hpp"

#include <QString>

// The keyboard and the clipboard: typed characters and pasted text become the same pieces and templates
// that the calculator's keys make, so the input stays two-dimensional however it was entered.
namespace typing {

// Types `c` at the cursor; false when it isn't typed at all (a control character).
bool typeCharacter(Entry& entry, QChar c);

// Ends a name typed just before the cursor: pi becomes π, Ans and M one piece each; other names stay letters.
void finishName(Entry& entry);

}  // namespace typing
