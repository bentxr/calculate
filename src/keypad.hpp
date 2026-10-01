#pragma once

#include <QList>
#include <QString>

enum class KeyAction { Insert, Clear, Backspace, Evaluate, MemoryAdd, MemorySubtract, MemoryClear, Left, Right, Up, Down };

// What a key does.
struct Face {
    QString label;     // the legend (English source text; see translated())
    QString insert;    // text inserted into the expression (Insert only; translated like the label)
    QString function;  // the engine function it stands for, if any (for Exact availability)
    KeyAction action = KeyAction::Insert;
};

struct Key {
    QString id;  // stable name: the button is "key:<id>" (right pad) or "direct:<id>" (left keyboard)
    Face face;
};

// The right pad, row by row: two rows of four keys with the cursor pad between their second and third
// keys, a row of six, and four rows of five (the numbers), laid out after the CASIO fx-991SP X II.
const QList<QList<Key>>& keypad();

// The cursor pad: up, left, right, down.
const QList<Key>& cursorPad();

// The left keyboard: every other function, on a key of its own, grouped by topic.
struct KeyGroup {
    QString title;  // English source text; see translated()
    QList<Key> keys;
};
const QList<KeyGroup>& directKeys();

// Whether the face works in the Exact type when `exact` (Exact refuses irrational functions).
bool available(const Face& face, bool exact);

// A label or inserted text in the user's language (Spanish calculators print sen, Arcsen, MCD…).
QString translated(const QString& text);
