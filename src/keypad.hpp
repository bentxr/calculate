#pragma once

#include "entry.hpp"

#include <QList>
#include <QString>
#include <QStringList>

enum class KeyAction { Insert, Template, Clear, Backspace, Evaluate, MemoryAdd, MemorySubtract, MemoryClear, MemoryStore, Left, Right, Up, Down };

// What a key does.
struct Face {
    QString label;     // the legend (English source text; see translated())
    QString insert;    // text inserted (Insert), or a template's filled-in box (Template: "2" for x²)
    QString function;  // the engine function it stands for, if any (for Exact availability)
    KeyAction action = KeyAction::Insert;
    Template shape = Template::Text;  // Template only
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

// The Statistics mode's functions, in their order on its page; each takes the values as arguments.
const QList<Key>& statisticsKeys();

// Whether the face works in the Exact type when `exact` (Exact refuses irrational functions).
bool available(const Face& face, bool exact);

// A label or inserted text in the user's language (Spanish calculators print sen, Arcsen, MCD…).
QString translated(const QString& text);
