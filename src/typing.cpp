#include "typing.hpp"

#include "settings.hpp"

#include <calculate-core/calculate-core.hpp>

#include <utility>
#include <vector>

namespace typing {

namespace {

// The engine's rule for names, ASCII only.
bool identifierCharacter(QChar c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool digit(QChar c) { return c >= '0' && c <= '9'; }

// The character of a one-character text piece, or 0 for anything else.
QChar character(const Item& item) {
    return item.kind == Template::Text && item.text.size() == 1 ? item.text[0] : QChar();
}

// The name that ends at the cursor, read as the engine's lexer would read the letters and digits
// before it (so the e of 1e10 is part of a number), or "" when a name doesn't end there.
QString nameBefore(const Entry& e, int* start) {
    const Row& row = e.currentRow();
    const int cursor = e.cursor();
    int first = cursor;
    while (first > 0 && identifierCharacter(character(row[static_cast<std::size_t>(first - 1)]))) --first;
    const auto at = [&](int i) { return i < cursor ? character(row[static_cast<std::size_t>(i)]) : QChar(); };
    int i = first;
    while (i < cursor) {
        const int tokenStart = i;
        if (digit(at(i)) || at(i) == '.') {
            while (digit(at(i))) ++i;
            if (at(i) == '.') ++i;
            while (digit(at(i))) ++i;
            if ((at(i) == 'e' || at(i) == 'E') && digit(at(i + 1))) {
                ++i;
                while (digit(at(i))) ++i;
            }
        } else {
            while (i < cursor && identifierCharacter(at(i))) ++i;
            if (i == cursor) {
                *start = tokenStart;
                QString name;
                for (int j = tokenStart; j < cursor; ++j) name += at(j);
                return name;
            }
        }
    }
    return {};
}

// Every name the engine knows, in every language the app ships, and whether it takes arguments.
std::vector<std::pair<QString, bool>> knownNames() {
    std::vector<std::pair<QString, bool>> names{{QStringLiteral("Ans"), false}, {QStringLiteral("M"), false}};
    for (const calculate_core::FunctionDescription& f : calculate_core::functions())
        for (const QString& spelling : settings::inEveryLanguage("keypad", QString::fromStdString(f.name) + "("))
            if (spelling.endsWith('(')) names.push_back({spelling.chopped(1), f.minArgs > 0});
    return names;
}

// Removes the letters from `start` to the cursor.
void removeName(Entry& e, int start) {
    while (e.cursor() > start) e.backspace();
}

// Replaces them with one piece.
void replaceName(Entry& e, int start, const QString& piece) {
    removeName(e, start);
    e.insert(piece);
}

// A call piece such as "nCr(" or "mcd(" whose function takes more than one argument.
bool takesSeveral(const QString& piece) {
    for (const calculate_core::FunctionDescription& f : calculate_core::functions()) {
        if (f.maxArgs >= 0 && f.maxArgs <= 1) continue;
        if (settings::inEveryLanguage("keypad", QString::fromStdString(f.name) + "(").contains(piece)) return true;
    }
    return false;
}

// The row after the first `depth` steps of the cursor's path.
const Row& rowAt(const Entry& e, std::size_t depth) {
    const Row* row = &e.root();
    for (std::size_t i = 0; i < depth; ++i) {
        const auto [item, box] = e.path()[i];
        row = &(*row)[static_cast<std::size_t>(item)].boxes[static_cast<std::size_t>(box)];
    }
    return *row;
}

bool opener(const Item& item) { return item.kind == Template::Text && item.text.endsWith('('); }
bool closer(const Item& item) { return item.kind == Template::Text && item.text == ")"; }

// The innermost opener before `end` that no ")" closes, or -1.
int openOpener(const Row& row, int end) {
    int depth = 0;
    for (int i = end; i-- > 0;) {
        const Item& item = row[static_cast<std::size_t>(i)];
        if (closer(item)) {
            ++depth;
        } else if (opener(item)) {
            if (depth == 0) return i;
            --depth;
        }
    }
    return -1;
}

bool balanced(const Row& row) {
    int openers = 0;
    for (const Item& item : row) openers += opener(item) ? 1 : closer(item) ? -1 : 0;
    return openers == 0;
}

// Whether a typed comma is an argument separator: the innermost open opener before the cursor is a call
// of a function that takes several arguments. A box that ends like linear text is part of its parent's
// text, so the search goes on there.
bool separatesArguments(const Entry& e) {
    std::size_t depth = e.path().size();
    int end = e.cursor();
    for (;;) {
        const Row& row = rowAt(e, depth);
        const int open = openOpener(row, end);
        if (open >= 0) {
            const QString& piece = row[static_cast<std::size_t>(open)].text;
            return piece != "(" && takesSeveral(piece);
        }
        if (depth == 0) return false;
        const int container = e.path()[depth - 1].first;
        if (rowAt(e, depth - 1)[static_cast<std::size_t>(container)].closing != Closing::Operand) return false;
        --depth;
        end = container;
    }
}

// The names that open a template, closed by a typed ")".
Template callTemplate(const QString& name) {
    if (name == "sqrt") return Template::Sqrt;
    if (name == "cbrt") return Template::Cbrt;
    if (name == "abs") return Template::Abs;
    if (name == "exp") return Template::Exp;
    return Template::Text;
}

bool operatorPiece(const Item& item) {
    return item.kind == Template::Text &&
           (item.text == "+" || item.text == QStringLiteral("−") || item.text == QStringLiteral("×") || item.text == QStringLiteral("÷"));
}

// Whether `piece` ends the box being typed, as it would end the operand in linear text: the cursor is at
// the end of a finished (non-empty, balanced) box that ends like linear text.
bool endsOperand(const Entry& e, const QString& piece) {
    if (e.hasSelection()) return false;  // the selection stays where it is, for the key to wrap
    const Item* container = e.container();
    const Row& row = e.currentRow();
    if (!container || container->closing != Closing::Operand) return false;
    if (row.empty() || e.cursor() != static_cast<int>(row.size()) || !balanced(row)) return false;
    if (piece == "^") return container->kind == Template::Sqrt || container->kind == Template::Cbrt;  // √ binds tighter
    if (piece == "+" || piece == QStringLiteral("−")) {
        const Item& last = row.back();
        if (operatorPiece(last) || opener(last) || last.text == ", ") return false;  // a sign
        const bool afterNumber = row.size() > 1 && (digit(character(row[row.size() - 2])) || character(row[row.size() - 2]) == '.');
        if ((last.text == "e" || last.text == "E") && afterNumber) return false;  // a number's exponent sign
        return true;
    }
    return piece == QStringLiteral("×") || piece == QStringLiteral("÷") || piece == ", " || piece == ")";
}

// The ")" that closes the opener at `open`, or -1.
int closerOf(const Row& row, int open) {
    int depth = 0;
    for (int i = open; i < static_cast<int>(row.size()); ++i) {
        const Item& item = row[static_cast<std::size_t>(i)];
        if (opener(item)) ++depth;
        else if (closer(item) && --depth == 0) return i;
    }
    return -1;
}

bool textIs(const Row& row, int i, const QString& text) {
    return i >= 0 && i < static_cast<int>(row.size()) && row[static_cast<std::size_t>(i)].kind == Template::Text &&
           row[static_cast<std::size_t>(i)].text == text;
}

Row slice(const Row& row, int from, int to) { return Row(row.begin() + from, row.begin() + to); }

// The template whose engine spelling ends with the ")" just typed, read back: ((N)/(D)), (10^(X)),
// root(A, B) and log(A, B) (the engine takes the radicand and the argument first).
void restore(Entry& e) {
    const Row& row = e.currentRow();
    const int end = e.cursor();  // just after the ")"
    const int o = openOpener(row, end - 1);
    if (o < 0) return;
    Item made;
    if (textIs(row, o, "(") && textIs(row, o + 1, "(")) {
        const int j = closerOf(row, o + 1);
        if (j < 0 || !textIs(row, j + 1, QStringLiteral("÷")) || !textIs(row, j + 2, "(")) return;
        const int k = closerOf(row, j + 2);
        if (k != end - 2) return;
        made = Item{Template::Fraction, {}, {slice(row, o + 2, j), slice(row, j + 3, k)}};
    } else if (end - o == 5 && textIs(row, o, "(") && textIs(row, o + 1, "1") && textIs(row, o + 2, "0") &&
               row[static_cast<std::size_t>(o + 3)].kind == Template::Power) {
        made = Item{Template::Pow10, {}, row[static_cast<std::size_t>(o + 3)].boxes};
    } else if (textIs(row, o, "root(") || textIs(row, o, "log(")) {
        int separator = -1;
        int depth = 0;
        for (int i = o + 1; i < end - 1; ++i) {
            const Item& item = row[static_cast<std::size_t>(i)];
            if (opener(item)) ++depth;
            else if (closer(item)) --depth;
            else if (depth == 0 && textIs(row, i, ", ")) {
                if (separator >= 0) return;  // three arguments: not a template
                separator = i;
            }
        }
        if (separator < 0) return;
        const Template kind = textIs(row, o, "root(") ? Template::Root : Template::LogBase;
        made = Item{kind, {}, {slice(row, separator + 1, end - 1), slice(row, o + 1, separator)}};
    } else {
        return;
    }
    e.replaceInRow(o, end, {made});
}

// Every box finished: from now on they behave like the keys' boxes.
void finishBoxes(Row& row) {
    for (Item& item : row) {
        item.closing = Closing::Key;
        for (Row& box : item.boxes) finishBoxes(box);
    }
}

// Inside a comment, after a # in the outer row, everything is typed as it is.
bool inComment(const Entry& e) {
    if (!e.path().empty()) return false;
    for (int i = 0; i < e.cursor(); ++i)
        if (textIs(e.root(), i, "#")) return true;
    return false;
}

void leaveOperands(Entry& e, const QString& piece) {
    while (endsOperand(e, piece)) e.right();
}

void insertPiece(Entry& e, const QString& piece) {
    leaveOperands(e, piece);
    e.insert(piece);
}

void openPower(Entry& e) {
    leaveOperands(e, "^");
    e.insertTemplate(Template::Power, Closing::Operand);
}

}  // namespace

void finishName(Entry& e) {
    int start = 0;
    const QString name = nameBefore(e, &start);
    if (name == "pi") replaceName(e, start, QStringLiteral("π"));
    else if (name == "Ans" || name == "M") replaceName(e, start, name);
}

QString nameBeingTyped(const Entry& e) {
    int start = 0;
    return nameBefore(e, &start);
}

QStringList completions(const QString& prefix) {
    QStringList list;
    if (prefix.isEmpty()) return list;
    for (const auto& [name, arguments] : knownNames())
        if (name.startsWith(prefix)) list << name;
    list.sort();
    list.removeDuplicates();
    return list;
}

void complete(Entry& e, const QString& name) {
    int start = e.cursor();
    if (!nameBefore(e, &start).isEmpty()) removeName(e, start);
    for (const QChar c : name) typeCharacter(e, c);
    bool arguments = false;
    for (const auto& [known, takes] : knownNames())
        if (known == name && takes) arguments = true;
    if (arguments) typeCharacter(e, '(');
    else finishName(e);
}

bool typeCharacter(Entry& e, QChar c) {
    if (!c.isPrint()) return false;
    if (inComment(e)) {
        e.insert(QString(c));
        return true;
    }
    if (c.isSpace() && c != ' ') return true;  // thin and no-break spaces group digits: ignored
    // Typing replaces a selection; an opening parenthesis and the template signs wrap it.
    const bool wraps = c == '(' || c == '^' || c == QChar(0x221A) || c == QChar(0x221B) || c == QChar(0x00B2) || c == QChar(0x00B3);
    if (e.hasSelection() && !wraps) e.backspace();
    if (!identifierCharacter(c) && !e.hasSelection()) finishName(e);
    const Row& row = e.currentRow();
    const Item* before = e.cursor() > 0 ? &row[static_cast<std::size_t>(e.cursor() - 1)] : nullptr;
    const Item* container = e.container();
    switch (c.unicode()) {
    case ' ':
        if (!before || !before->text.endsWith(' ')) e.insert(" ");
        break;
    case '*':
        if (before && before->text == QStringLiteral("×")) {  // ** is a power
            e.backspace();
            openPower(e);
        } else {
            insertPiece(e, QStringLiteral("×"));
        }
        break;
    case '/': insertPiece(e, QStringLiteral("÷")); break;
    case '-': insertPiece(e, QStringLiteral("−")); break;
    case '+': insertPiece(e, "+"); break;
    case ';': insertPiece(e, ", "); break;
    case ',':  // with a decimal comma, always the decimal point; `;` separates
        insertPiece(e, !settings::decimalComma() && separatesArguments(e) ? QStringLiteral(", ") : QStringLiteral("."));
        break;
    case '^': openPower(e); break;
    case 0x221A: e.insertTemplate(Template::Sqrt, Closing::Operand); break;  // √
    case 0x221B: e.insertTemplate(Template::Cbrt, Closing::Operand); break;  // ∛
    case 0x00B2:  // ², as the x² key
    case 0x00B3:
        e.insertTemplate(Template::Power);
        e.insert(c == QChar(0x00B2) ? "2" : "3");
        e.right();
        break;
    case '(': {
        if (e.hasSelection()) {  // wraps it, whatever comes before
            e.insert("(");
            break;
        }
        if (container && container->closing == Closing::Operand && row.empty()) {  // the box's own parentheses
            e.setClosing(Closing::Parenthesis);
            break;
        }
        int start = 0;
        const QString name = nameBefore(e, &start);
        const Template kind = callTemplate(name);
        if (name.isEmpty()) {
            e.insert("(");
        } else if (kind != Template::Text) {
            removeName(e, start);
            e.insertTemplate(kind, Closing::Parenthesis);
        } else {
            replaceName(e, start, name + "(");
        }
        break;
    }
    case ')':
        leaveOperands(e, ")");
        if (e.container() && e.container()->closing == Closing::Parenthesis && e.cursor() == static_cast<int>(e.currentRow().size()) &&
            balanced(e.currentRow()))
            e.right();
        else {
            e.insert(")");
            restore(e);
        }
        break;
    case '>':
        if (before && before->text == QStringLiteral("−")) {
            e.backspace();
            e.insert(QStringLiteral("→"));
        } else {
            e.insert(">");
        }
        break;
    default:
        // A word after a space is not part of the operand (2^10 to …): the space and the word go outside.
        if (c.isLetter() && container && container->closing == Closing::Operand && before && before->text == " " &&
            e.cursor() == static_cast<int>(row.size())) {
            e.backspace();
            e.right();
            e.insert(" ");
        }
        e.insert(QString(c));
    }
    return true;
}

Row read(const QString& text) {
    Entry e;
    for (const QChar c : text) typeCharacter(e, c);
    finishName(e);
    Row row = e.root();
    finishBoxes(row);
    return row;
}

void paste(Entry& e, const QString& text) { e.insertRow(read(text)); }

}  // namespace typing
