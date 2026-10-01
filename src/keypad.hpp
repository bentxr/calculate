#pragma once

#include <QList>
#include <QString>

enum class KeyAction {
    Insert, Clear, Backspace, Evaluate, MemoryAdd, MemorySubtract, MemoryClear,
    Shift, Alpha, Menu, Config, Options, Left, Right, Up, Down,
    Unavailable,  // printed on the calculator, not supported here yet
};

// One function of a key: the key alone, after SHIFT, or after ALPHA.
struct Face {
    QString label;     // the legend (English source text; see translated())
    QString insert;    // text inserted into the expression (Insert only; translated like the label)
    QString function;  // the engine function it stands for, if any (for Exact availability)
    KeyAction action = KeyAction::Unavailable;
};

struct Key {
    QString id;  // stable name: the button is "key:<id>"
    Face main;
    Face shift;  // the yellow legend; empty when there is none
    Face alpha;  // the red legend; empty when there is none
};

// The keys of the CASIO fx-991SP X II, row by row. The first two rows have four keys each, with the
// cursor pad between their second and third keys; then three rows of six keys and four rows of five.
const QList<QList<Key>>& keypad();

// The cursor pad: up, left, right, down.
const QList<Key>& cursorPad();

// The OPTN menu: the hyperbolic functions (in a menu on the Casio too), mod and memory clear.
const QList<Face>& optionsMenu();

// The second keyboard: every function the Casio hides behind SHIFT, ALPHA or OPTN, on a key of its
// own and grouped by topic, for anyone who doesn't know the calculator.
struct KeyGroup {
    QString title;  // English source text; see translated()
    QList<Key> keys;
};
const QList<KeyGroup>& directKeys();

// Whether the face does something in this app, and in the Exact type when `exact`.
bool available(const Face& face, bool exact);

// A label or inserted text in the user's language (Spanish calculators print sen, Arcsen, MCD…).
QString translated(const QString& text);
