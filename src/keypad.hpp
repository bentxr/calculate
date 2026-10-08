#pragma once

#include "entry.hpp"

#include <QList>
#include <QString>
#include <QStringList>

enum class KeyAction { Insert, Template, Clear, Backspace, Evaluate, MemoryAdd, MemorySubtract, MemoryClear, MemoryStore, Left, Right, Up, Down,
                       Type,    // the inserted text goes through the typing rules, character by character
                       Shift,  // the next letter is a capital
                       Undo, Redo,
                       Tool,    // opens one of the rail's tools
                       Store };  // the next variable letter stores the input under its name

// What a key does.
struct Face {
    QString label;     // the legend (English source text; see translated())
    QString insert;    // text inserted (Insert), or a template's filled-in box (Template: "2" for x²)
    QString function;  // the engine function it stands for, if any (for Exact availability)
    KeyAction action = KeyAction::Insert;
    Template shape = Template::Text;  // Template only
    QString opens;  // Tool: the tool it opens
};

struct Key {
    QString id;  // stable name: the button is "key:<id>" (right pad) or "direct:<id>" (left keyboard)
    Face face;
};

// The right pad, row by row: two rows of four keys with the cursor pad between their second and third
// keys, a row of six, and four rows of five (the numbers).
const QList<QList<Key>>& keypad();

// The cursor pad: up, left, right, down.
const QList<Key>& cursorPad();

// The left column. Always shown: Common, the keys used most (each the twin of a section key; the user may change
// them), and Memory and editing. Then titled sections that open in place, each the home of its keys.
struct KeySection {
    QString id;     // "section:<id>" is its header, "sectionKeys:<id>" its keys
    QString title;  // English source text; see translated()
    QList<Key> keys;
};
const QList<KeySection>& keySections();
const QList<Key>& memoryKeys();
const QStringList& defaultCommon();  // the ids of Common's keys at start, in order
QList<Key> everyDirectKey();          // Memory and editing, then every section's keys, in order
Key directKey(const QString& id);     // the key of the column with this id; an empty Key when there is none

inline constexpr int commonLimit = 12;  // two rows of six
// Whether `ids` can be Common: keys of the sections (the main pad and Memory and editing are always in view),
// each once, at most commonLimit of them.
bool canBeCommon(const QStringList& ids);

// The Statistics mode's functions, in their order on its page; each takes the values as arguments.
const QList<Key>& statisticsKeys();

// An entry of the search list: what a key (or, from Plan 3, a name without a key) types, under a heading.
struct SearchEntry {
    QString group;  // the heading: a section's title, or "Main keys" (English source text; see translated())
    Face face;
    QString title;  // a one-line description (Plan 2's metadata fills it)
};
// The keys of every section but Letters, under the section's title and in order; then the main pad's keys that
// stand for a function, under "Main keys"; then extraSearchEntries().
QList<SearchEntry> searchEntries();
const QList<SearchEntry>& extraSearchEntries();  // what has no key of its own (empty until Plan 3)
// Whether an entry matches the search box's text: its legend (as written or translated), what it types, its
// function, its description or its heading, ignoring case. An empty text matches everything.
bool searchMatches(const SearchEntry& entry, const QString& text);

// Whether the face works in the Exact type when `exact` (Exact refuses irrational functions).
bool available(const Face& face, bool exact);

// The keys a key also offers on a long press or a right click: a main pad key's id, then the ids of the column's keys
// it offers (sin → asin, sinh, asinh). A shortcut only: every one of them has its own key too.
const QList<QPair<QString, QStringList>>& alternates();

// The key's name for screen readers, in the user's language: words for symbol legends (x⁻¹ "reciprocal"); any
// other legend speaks as itself.
QString spokenName(const Key& key);

// A label or inserted text in the user's language (Spanish calculators print sen, Arcsen, MCD…).
QString translated(const QString& text);
// A key's legend as shown: translated, and with the decimal comma the point key shows "," and the separator ";".
QString legend(const Face& face, bool decimalComma);
