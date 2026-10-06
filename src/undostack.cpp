#include "undostack.hpp"

void UndoStack::record(const Entry& before) {
    redo_.clear();
    undo_.push_back(before);
    if (undo_.size() > limit) undo_.erase(undo_.begin());
}

bool UndoStack::undo(Entry& entry) {
    if (undo_.empty()) return false;
    redo_.push_back(entry);
    entry = undo_.back();
    undo_.pop_back();
    return true;
}

bool UndoStack::redo(Entry& entry) {
    if (redo_.empty()) return false;
    undo_.push_back(entry);
    entry = redo_.back();
    redo_.pop_back();
    return true;
}
