#include "typing.hpp"

#include "settings.hpp"

#include <calculate-core/calculate-core.hpp>

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

// Replaces the letters from `start` to the cursor with one piece.
void replaceName(Entry& e, int start, const QString& piece) {
    while (e.cursor() > start) e.backspace();
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

// Whether a typed comma is an argument separator: the innermost open opener before the cursor is a call
// of a function that takes several arguments.
bool separatesArguments(const Entry& e) {
    const Row& row = e.currentRow();
    int depth = 0;
    for (int i = e.cursor(); i-- > 0;) {
        const Item& item = row[static_cast<std::size_t>(i)];
        if (item.kind != Template::Text) continue;
        if (item.text == ")") {
            ++depth;
        } else if (item.text.endsWith('(')) {
            if (depth == 0) return item.text != "(" && takesSeveral(item.text);
            --depth;
        }
    }
    return false;
}

}  // namespace

void finishName(Entry& e) {
    int start = 0;
    const QString name = nameBefore(e, &start);
    if (name == "pi") replaceName(e, start, QStringLiteral("π"));
    else if (name == "Ans" || name == "M") replaceName(e, start, name);
}

bool typeCharacter(Entry& e, QChar c) {
    if (!c.isPrint()) return false;
    if (c.isSpace() && c != ' ') return true;  // thin and no-break spaces group digits: ignored
    if (!identifierCharacter(c)) finishName(e);
    const Row& row = e.currentRow();
    switch (c.unicode()) {
    case ' ':
        if (e.cursor() == 0 || !row[static_cast<std::size_t>(e.cursor() - 1)].text.endsWith(' ')) e.insert(" ");
        break;
    case '*': e.insert(QStringLiteral("×")); break;
    case '/': e.insert(QStringLiteral("÷")); break;
    case '-': e.insert(QStringLiteral("−")); break;
    case ';': e.insert(", "); break;
    case ',': e.insert(separatesArguments(e) ? QStringLiteral(", ") : QStringLiteral(".")); break;
    case '(': {
        int start = 0;
        const QString name = nameBefore(e, &start);
        if (name.isEmpty()) e.insert("(");
        else replaceName(e, start, name + "(");
        break;
    }
    default: e.insert(QString(c));
    }
    return true;
}

}  // namespace typing
