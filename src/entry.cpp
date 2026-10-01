#include "entry.hpp"

#include <QtGlobal>

namespace {

int boxCount(Template kind) {
    switch (kind) {
    case Template::Text: return 0;
    case Template::Fraction:
    case Template::Root:
    case Template::LogBase: return 2;
    default: return 1;
    }
}

QString serialize(const Row& row);

// Templates become plain engine text. Fractions and 10ˣ get parentheses of their own, so that a
// template typed right after a digit is an error for the engine and never merges with it.
QString serialize(const Item& item) {
    const auto box = [&](int i) { return serialize(item.boxes[static_cast<std::size_t>(i)]); };
    switch (item.kind) {
    case Template::Text: return item.text;
    case Template::Fraction: return "((" + box(0) + ")/(" + box(1) + "))";
    case Template::Sqrt: return "√(" + box(0) + ")";
    case Template::Cbrt: return "∛(" + box(0) + ")";
    case Template::Root: return "root(" + box(1) + ", " + box(0) + ")";
    case Template::Power: return "^(" + box(0) + ")";
    case Template::Exp: return "exp(" + box(0) + ")";
    case Template::Pow10: return "(10^(" + box(0) + "))";
    case Template::LogBase: return "log(" + box(1) + ", " + box(0) + ")";  // the engine takes the base second
    case Template::Abs: return "abs(" + box(0) + ")";
    }
    return {};
}

QString serialize(const Row& row) {
    QString s;
    for (const Item& item : row) s += serialize(item);
    return s;
}

}  // namespace

Row& Entry::rowAt(std::size_t depth) {
    Row* r = &root_;
    for (std::size_t i = 0; i < depth; ++i) r = &(*r)[static_cast<std::size_t>(path_[i].first)].boxes[static_cast<std::size_t>(path_[i].second)];
    return *r;
}

void Entry::insert(const QString& piece) {
    Row& r = row();
    r.insert(r.begin() + index_, Item{Template::Text, piece, {}});
    ++index_;
}

void Entry::insertTemplate(Template kind) {
    Row& r = row();
    r.insert(r.begin() + index_, Item{kind, {}, std::vector<Row>(static_cast<std::size_t>(boxCount(kind)))});
    path_.emplace_back(index_, 0);
    index_ = 0;
}

void Entry::backspace() {
    if (index_ > 0) {
        Row& r = row();
        r.erase(r.begin() + --index_);
        return;
    }
    if (path_.empty()) return;
    const auto [item, box] = path_.back();
    if (box > 0) {  // a later box: back to the end of the previous one
        left();
        return;
    }
    // The first box: the template goes; what was typed in its boxes stays, in screen order (a fraction
    // keeps a ÷ between its parts, so it still means the same).
    Row& parent = rowAt(path_.size() - 1);
    const Item gone = parent[static_cast<std::size_t>(item)];
    Row kept;
    for (std::size_t i = 0; i < gone.boxes.size(); ++i) {
        if (i > 0 && gone.kind == Template::Fraction) kept.push_back(Item{Template::Text, QStringLiteral("÷"), {}});
        kept.insert(kept.end(), gone.boxes[i].begin(), gone.boxes[i].end());
    }
    parent.erase(parent.begin() + item);
    parent.insert(parent.begin() + item, kept.begin(), kept.end());
    path_.pop_back();
    index_ = item;
}

// Into a template's last box, from one box to the previous, and out before the template.
void Entry::left() {
    Row& r = row();
    if (index_ > 0) {
        const Item& before = r[static_cast<std::size_t>(index_ - 1)];
        if (before.kind == Template::Text) {
            --index_;
        } else {
            const int last = static_cast<int>(before.boxes.size()) - 1;
            path_.emplace_back(index_ - 1, last);
            index_ = static_cast<int>(before.boxes[static_cast<std::size_t>(last)].size());
        }
        return;
    }
    if (path_.empty()) return;
    const auto [item, box] = path_.back();
    if (box > 0) {
        path_.back().second = box - 1;
        index_ = static_cast<int>(row().size());
    } else {
        path_.pop_back();
        index_ = item;
    }
}

bool Entry::up() { return moveInFraction(0); }
bool Entry::down() { return moveInFraction(1); }

// To the numerator (0) or the denominator (1) of the innermost fraction around the cursor.
bool Entry::moveInFraction(int box) {
    for (std::size_t depth = path_.size(); depth-- > 0;) {
        if (rowAt(depth)[static_cast<std::size_t>(path_[depth].first)].kind != Template::Fraction) continue;
        if (path_[depth].second != box) {
            path_.resize(depth + 1);
            path_[depth].second = box;
            index_ = qMin(index_, static_cast<int>(row().size()));
        }
        return true;
    }
    return false;
}

// Into a template's first box, from one box to the next, and out after the template.
void Entry::right() {
    Row& r = row();
    if (index_ < static_cast<int>(r.size())) {
        if (r[static_cast<std::size_t>(index_)].kind == Template::Text) ++index_;
        else {
            path_.emplace_back(index_, 0);
            index_ = 0;
        }
        return;
    }
    if (path_.empty()) return;
    const auto [item, box] = path_.back();
    const Item& parent = rowAt(path_.size() - 1)[static_cast<std::size_t>(item)];
    if (box + 1 < static_cast<int>(parent.boxes.size())) {
        path_.back().second = box + 1;
        index_ = 0;
    } else {
        path_.pop_back();
        index_ = item + 1;
    }
}

void Entry::clear() {
    root_.clear();
    path_.clear();
    index_ = 0;
}

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
        root_.push_back(Item{Template::Text, text.mid(i, end - i), {}});
        i = end;
    }
    index_ = static_cast<int>(root_.size());
}

QString Entry::text() const { return serialize(root_); }
