#include "entry.hpp"

void Entry::insert(const QString& piece) {
    pieces_.insert(cursor_, piece);
    ++cursor_;
}

void Entry::backspace() {
    if (cursor_ == 0) return;
    pieces_.removeAt(--cursor_);
}

void Entry::left() {
    if (cursor_ > 0) --cursor_;
}

void Entry::right() {
    if (cursor_ < pieces_.size()) ++cursor_;
}

void Entry::clear() {
    pieces_.clear();
    cursor_ = 0;
}

// The pieces the keys would have made: a name with its "(" ("sin("), a name alone ("Ans"), or one
// character; spaces stay with the piece before them (", ").
void Entry::setText(const QString& text) {
    clear();
    int i = 0;
    while (i < text.size()) {
        int end = i + 1;
        if (text[i].isLetter()) {
            while (end < text.size() && text[end].isLetter()) ++end;
            if (end < text.size() && text[end] == '(') ++end;
        }
        while (end < text.size() && text[end] == ' ') ++end;
        pieces_ << text.mid(i, end - i);
        i = end;
    }
    cursor_ = pieces_.size();
}
