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

// The row after the first `depth` steps of `path` (Row or const Row).
template <typename R>
R& walk(R& root, const std::vector<std::pair<int, int>>& path, std::size_t depth) {
    R* r = &root;
    for (std::size_t i = 0; i < depth; ++i) r = &(*r)[static_cast<std::size_t>(path[i].first)].boxes[static_cast<std::size_t>(path[i].second)];
    return *r;
}

}  // namespace

bool operator==(const Item& a, const Item& b) {
    return a.kind == b.kind && a.text == b.text && a.boxes == b.boxes && a.closing == b.closing;
}
bool operator!=(const Item& a, const Item& b) { return !(a == b); }

bool operator==(const Position& a, const Position& b) { return a.path == b.path && a.index == b.index; }
bool operator!=(const Position& a, const Position& b) { return !(a == b); }

Row& Entry::rowAt(std::size_t depth) { return walk(root_, path_, depth); }

const Row& Entry::currentRow() const { return walk(root_, path_, path_.size()); }

const Item* Entry::container() const {
    if (path_.empty()) return nullptr;
    return &walk(root_, path_, path_.size() - 1)[static_cast<std::size_t>(path_.back().first)];
}

Item& Entry::containerItem() { return rowAt(path_.size() - 1)[static_cast<std::size_t>(path_.back().first)]; }

void Entry::setClosing(Closing closing) {
    if (!path_.empty()) containerItem().closing = closing;
}

// Leaving a box ends its typing: from now on it behaves like one a key made.
void Entry::stepOut() {
    containerItem().closing = Closing::Key;
    index_ = path_.back().first + 1;
    path_.pop_back();
}

void Entry::insert(const QString& piece) {
    const Row selection = removeSelection();
    Row& r = row();
    if (!selection.empty() && piece.endsWith('(')) {  // wrap: piece, the selection, ")"
        Row wrapped{Item{Template::Text, piece, {}}};
        wrapped.insert(wrapped.end(), selection.begin(), selection.end());
        wrapped.push_back(Item{Template::Text, QStringLiteral(")"), {}});
        r.insert(r.begin() + index_, wrapped.begin(), wrapped.end());
        index_ += static_cast<int>(wrapped.size());
        return;
    }
    r.insert(r.begin() + index_, Item{Template::Text, piece, {}});
    ++index_;
}

void Entry::insertRow(const Row& items) {
    removeSelection();
    Row& r = row();
    r.insert(r.begin() + index_, items.begin(), items.end());
    index_ += static_cast<int>(items.size());
}

void Entry::insertTemplate(Template kind, Closing closing) {
    Row selection = removeSelection();
    if (kind == Template::Power && !selection.empty()) {  // the base, before the power
        if (selection.size() > 1) {
            selection.insert(selection.begin(), Item{Template::Text, QStringLiteral("("), {}});
            selection.push_back(Item{Template::Text, QStringLiteral(")"), {}});
        }
        insertRow(selection);
        selection.clear();
    }
    Item made{kind, {}, std::vector<Row>(static_cast<std::size_t>(boxCount(kind))), closing};
    int box = 0;  // where the cursor goes; -1: after the template, which is done
    if (!selection.empty()) {
        switch (kind) {
        case Template::Fraction:  // the numerator; on to the denominator
            made.boxes[0] = selection;
            box = 1;
            break;
        case Template::Root:  // the radicand or the argument; the index or the base is still to type
        case Template::LogBase: made.boxes[1] = selection; break;
        default:
            made.boxes[0] = selection;
            made.closing = Closing::Key;
            box = -1;
        }
    }
    Row& r = row();
    r.insert(r.begin() + index_, made);
    if (box < 0) {
        ++index_;
        return;
    }
    path_.emplace_back(index_, box);
    index_ = 0;
}

Row Entry::removeSelection() {
    if (!hasSelection()) {
        anchor_ = -1;
        return {};
    }
    const Row taken = selected();
    Row& r = row();
    const int from = qMin(anchor_, index_);
    r.erase(r.begin() + from, r.begin() + qMax(anchor_, index_));
    index_ = from;
    anchor_ = -1;
    return taken;
}

void Entry::backspace() {
    if (hasSelection()) {
        removeSelection();
        return;
    }
    anchor_ = -1;
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
    if (hasSelection()) {  // a selection collapses to its start
        index_ = qMin(anchor_, index_);
        anchor_ = -1;
        return;
    }
    anchor_ = -1;
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
        stepOut();
        index_ = item;
    }
}

bool Entry::up() { return moveInFraction(0); }
bool Entry::down() { return moveInFraction(1); }

// To the numerator (0) or the denominator (1) of the innermost fraction around the cursor.
bool Entry::moveInFraction(int box) {
    anchor_ = -1;
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
    if (hasSelection()) {
        index_ = qMax(anchor_, index_);
        anchor_ = -1;
        return;
    }
    anchor_ = -1;
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
        stepOut();
    }
}

void Entry::home() {
    anchor_ = -1;
    path_.clear();
    index_ = 0;
}

void Entry::end() {
    anchor_ = -1;
    path_.clear();
    index_ = static_cast<int>(root_.size());
}

void Entry::deleteForward() {
    if (hasSelection()) {
        removeSelection();
        return;
    }
    anchor_ = -1;
    Row& r = row();
    if (index_ < static_cast<int>(r.size())) r.erase(r.begin() + index_);
}

void Entry::clear() {
    anchor_ = -1;
    root_.clear();
    path_.clear();
    index_ = 0;
}

void Entry::setRoot(Row root) {
    root_ = std::move(root);
    end();
}

void Entry::replaceInRow(int from, int to, const Row& items) {
    anchor_ = -1;
    Row& r = row();
    r.erase(r.begin() + from, r.begin() + to);
    r.insert(r.begin() + from, items.begin(), items.end());
    index_ = from + static_cast<int>(items.size());
}

void Entry::setPosition(const Position& p) {
    anchor_ = -1;
    path_ = p.path;
    index_ = qBound(0, p.index, static_cast<int>(row().size()));
}

bool Entry::operator==(const Entry& other) const {
    return root_ == other.root_ && path_ == other.path_ && index_ == other.index_ && anchor_ == other.anchor_;
}

Row Entry::selected() const {
    if (!hasSelection()) return {};
    const Row& r = currentRow();
    return Row(r.begin() + qMin(anchor_, index_), r.begin() + qMax(anchor_, index_));
}

QString Entry::selectedText() const { return serialize(selected()); }

void Entry::extendLeft() {
    if (anchor_ < 0) anchor_ = index_;
    if (index_ > 0) {
        --index_;
    } else if (!path_.empty()) {  // past the box's edge: the whole template
        const int item = path_.back().first;
        path_.pop_back();
        anchor_ = item + 1;
        index_ = item;
    }
}

void Entry::extendRight() {
    if (anchor_ < 0) anchor_ = index_;
    if (index_ < static_cast<int>(row().size())) {
        ++index_;
    } else if (!path_.empty()) {
        const int item = path_.back().first;
        path_.pop_back();
        anchor_ = item;
        index_ = item + 1;
    }
}

void Entry::extendHome() {
    if (anchor_ < 0) anchor_ = index_;
    if (!path_.empty()) {
        anchor_ = path_[0].first + 1;
        path_.clear();
    }
    index_ = 0;
}

void Entry::extendEnd() {
    if (anchor_ < 0) anchor_ = index_;
    if (!path_.empty()) {
        anchor_ = path_[0].first;
        path_.clear();
    }
    index_ = static_cast<int>(root_.size());
}

void Entry::selectAll() {
    path_.clear();
    anchor_ = 0;
    index_ = static_cast<int>(root_.size());
}

void Entry::select(const Position& from, const Position& to) {
    if (from == to) {
        setPosition(to);
        return;
    }
    std::size_t k = 0;  // the steps both paths share: the selection lives in that row
    while (k < from.path.size() && k < to.path.size() && from.path[k] == to.path[k]) ++k;
    // Each end, as the items it covers in that row: a place deeper down covers its whole template.
    const auto first = [k](const Position& p) { return p.path.size() > k ? p.path[k].first : p.index; };
    const auto last = [k](const Position& p) { return p.path.size() > k ? p.path[k].first + 1 : p.index; };
    path_.assign(from.path.begin(), from.path.begin() + static_cast<std::ptrdiff_t>(k));
    const int low = qMin(first(from), first(to));
    const int high = qMax(last(from), last(to));
    const bool backwards = first(to) < first(from) || (first(to) == first(from) && last(to) < last(from));
    index_ = backwards ? low : high;
    anchor_ = backwards ? high : low;
}

QString Entry::text() const { return serialize(root_); }
