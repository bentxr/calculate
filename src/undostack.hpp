#pragma once

#include "entry.hpp"

#include <cstddef>
#include <vector>

// Earlier and later states of the input, for undo and redo. Every edit records the state before it.
class UndoStack {
public:
    static constexpr std::size_t limit = 100;
    void record(const Entry& before);  // drops the states that could be redone; keeps the last `limit`
    bool undo(Entry& entry);           // false when there is nothing to undo
    bool redo(Entry& entry);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

private:
    std::vector<Entry> undo_, redo_;
};
