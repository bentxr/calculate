#pragma once

#include <QList>
#include <QString>

enum class KeyAction { Insert, Clear, Backspace, Evaluate, MemoryAdd, MemorySubtract, MemoryClear };

struct Key {
    QString label;
    QString insert;     // text inserted into the expression (Insert only)
    QString function;   // the language function it stands for, if any (for Exact availability)
    KeyAction action = KeyAction::Insert;
    int span = 1;       // grid columns
};

// The keypad, row by row: rows of keypadColumns columns.
const QList<Key>& keypad();
constexpr int keypadColumns = 8;

// Whether the Exact type can evaluate the key's function.
bool availableInExact(const Key& key);
