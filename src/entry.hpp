#pragma once

#include <QString>

#include <utility>
#include <vector>

// The calculator's two-dimensional templates (its MathIO), and plain text.
enum class Template { Text, Fraction, Sqrt, Cbrt, Root, Power, Exp, Pow10, LogBase, Abs };

// How a template's box ends while it is being typed: as a key made it (only ► leaves it), at a typed ")"
// (opened by typing sqrt( and the like), or where the operand of linear text ends (opened by typing ^ or √).
// A box the cursor has left is always Key again.
enum class Closing { Key, Parenthesis, Operand };

// One thing in a row: what a key typed ("7", "×", "sin("), or a template with boxes of its own, in
// screen order: numerator then denominator, index then radicand, base then argument.
struct Item {
    Template kind = Template::Text;
    QString text;                          // Text only
    std::vector<std::vector<Item>> boxes;  // templates only
    Closing closing = Closing::Key;
};
using Row = std::vector<Item>;

bool operator==(const Item& a, const Item& b);
bool operator!=(const Item& a, const Item& b);

// A place for the cursor: the row reached by `path` (as Entry::path()) and the index in that row.
struct Position {
    std::vector<std::pair<int, int>> path;
    int index = 0;
};
bool operator==(const Position& a, const Position& b);
bool operator!=(const Position& a, const Position& b);

// What the user has typed, as the calculator shows it: a row of items with a cursor between two of
// them, possibly inside a template's box. A key's text is one item, so DEL removes it whole.
class Entry {
public:
    void insert(const QString& piece);   // at the cursor, which moves past it
    void insertTemplate(Template kind, Closing closing = Closing::Key);  // the cursor goes into its first box
    void insertRow(const Row& items);  // at the cursor, which moves past them
    // Removes the item before the cursor. At the start of a template's first box it removes the
    // template and keeps what was typed in it; at the start of a later box it goes back a box.
    void backspace();
    void left();
    void right();
    // Inside a fraction, between numerator and denominator (and true); false outside one, where ▲ and
    // ▼ belong to the history.
    bool up();
    bool down();
    void home();  // the start of the whole input
    void end();   // its end
    void deleteForward();  // removes the item after the cursor, a whole template included
    void clear();
    void setRoot(Row root);  // replaces the content; the cursor goes to the end
    // Replaces the items [from, to) of the cursor's row with `items`; the cursor goes after them.
    void replaceInRow(int from, int to, const Row& items);

    Position position() const { return {path_, index_}; }
    void setPosition(const Position& p);  // the index is kept inside its row

    bool isEmpty() const { return root_.empty(); }
    QString text() const;        // the expression for the engine
    int cursor() const { return index_; }  // the place in the cursor's row
    const Row& root() const { return root_; }
    const Row& currentRow() const;  // the cursor's row
    const Item* container() const;  // the innermost template around the cursor; nullptr at the root
    void setClosing(Closing closing);  // of that template
    // How to reach the cursor's row from the outer one: (item, box) at each level.
    const std::vector<std::pair<int, int>>& path() const { return path_; }

    bool operator==(const Entry& other) const;
    bool operator!=(const Entry& other) const { return !(*this == other); }

private:
    Row& rowAt(std::size_t depth);  // the row after the first `depth` steps of the path
    Row& row() { return rowAt(path_.size()); }
    Item& containerItem();
    void stepOut();  // to just after the container, which is finished
    bool moveInFraction(int box);

    Row root_;
    std::vector<std::pair<int, int>> path_;
    int index_ = 0;
};
