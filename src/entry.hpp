#pragma once

#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

// The calculator's two-dimensional templates (its MathIO), and plain text.
enum class Template { Text, Fraction, Sqrt, Cbrt, Root, Power, Exp, Pow10, LogBase, Abs };

// One thing in a row: what a key typed ("7", "×", "sin("), or a template with boxes of its own, in
// screen order: numerator then denominator, index then radicand, base then argument.
struct Item {
    Template kind = Template::Text;
    QString text;                          // Text only
    std::vector<std::vector<Item>> boxes;  // templates only
};
using Row = std::vector<Item>;

// What the user has typed, as the calculator shows it: a row of items with a cursor between two of
// them, possibly inside a template's box. A key's text is one item, so DEL removes it whole.
class Entry {
public:
    void insert(const QString& piece);   // at the cursor, which moves past it
    void insertTemplate(Template kind);  // the cursor goes into its first box
    void backspace();                    // removes the item before the cursor
    void left();
    void right();
    void clear();
    // Replaces the content with `text`, split into the pieces the keys would make; the cursor goes
    // to the end. (Text from the history or the statistics page is always one row.)
    void setText(const QString& text);

    bool isEmpty() const { return root_.empty(); }
    QString text() const;        // the expression for the engine
    QStringList pieces() const;  // the outer row, one string per item
    int cursor() const { return index_; }  // the place in the cursor's row
    const Row& root() const { return root_; }
    // How to reach the cursor's row from the outer one: (item, box) at each level.
    const std::vector<std::pair<int, int>>& path() const { return path_; }

private:
    Row& rowAt(std::size_t depth);  // the row after the first `depth` steps of the path
    Row& row() { return rowAt(path_.size()); }

    Row root_;
    std::vector<std::pair<int, int>> path_;
    int index_ = 0;
};
