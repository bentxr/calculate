#pragma once

#include "entry.hpp"

#include <QString>
#include <QStringList>

// The keyboard and the clipboard: typed characters and pasted text become the same pieces and templates
// that the calculator's keys make, so the input stays two-dimensional however it was entered.
namespace typing {

// Types `c` at the cursor; false when it isn't typed at all (a control character).
bool typeCharacter(Entry& entry, QChar c);

// Ends a name typed just before the cursor: pi becomes π, Ans and M one piece each; other names stay letters.
void finishName(Entry& entry);

// What typing `text` would give, every box finished.
Row read(const QString& text);

// Inserts read(text) at the cursor, replacing a selection; the cursor goes after it.
void paste(Entry& entry, const QString& text);

// The name that ends at the cursor (letters and digits, as the engine reads them), or "".
QString nameBeingTyped(const Entry& entry);
// Every name that starts with `prefix` (case-sensitive, as the engine): functions and constants in every
// language the app ships, Ans and M; sorted, none for an empty prefix.
QStringList completions(const QString& prefix);
// The call the cursor is in: the function's name as typed ("nCr", "sen"; sum and product for Σ and Π) and which of
// its arguments the cursor is in, from 0. Plain parentheses around the cursor are looked through. An empty name
// outside every call.
struct Call {
    QString name;
    int argument = 0;
};
Call callAround(const Entry& entry);

// The title of the function `name` (or that spelling) stands for, in the user's language; "" for other names.
QString completionTitle(const QString& name);
// Replaces the name being typed with `name`, and opens its call when it takes arguments.
void complete(Entry& entry, const QString& name);

}  // namespace typing
