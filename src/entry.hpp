#pragma once

#include <QString>
#include <QStringList>

// What the user has typed: pieces with a cursor between them. A piece is what one key inserts
// ("7", "×", "sin("), so DEL removes it whole and the cursor moves over it in one step, as on the
// calculator.
class Entry {
public:
    void insert(const QString& piece);  // at the cursor, which moves past it
    void backspace();                   // removes the piece before the cursor
    void left();
    void right();
    void clear();
    // Replaces the content with `text`, split into pieces; the cursor goes to the end.
    void setText(const QString& text);

    bool isEmpty() const { return pieces_.isEmpty(); }
    QString text() const { return pieces_.join(QString()); }  // the expression for the engine
    const QStringList& pieces() const { return pieces_; }
    int cursor() const { return cursor_; }  // the number of pieces before it

private:
    QStringList pieces_;
    int cursor_ = 0;
};
