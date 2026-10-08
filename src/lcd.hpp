#pragma once

#include "entry.hpp"
#include "presenter.hpp"
#include "typeset.hpp"
#include "undostack.hpp"

#include <QWidget>

#include <functional>

class QHBoxLayout;
class QMenu;
class QScrollBar;

// The calculator's screen: the input at the top and the result at the bottom right, drawn in two
// dimensions. It owns the keyboard: everything typed goes through typing::typeCharacter, so a typed
// name or sign becomes the same piece or template its key makes; Enter, Backspace, Delete, Esc, the
// arrows, Home and End edit.
class Lcd : public QWidget {
    Q_OBJECT

public:
    explicit Lcd(QWidget* parent = nullptr);

    QString input() const { return entry_.text(); }
    const Entry& entry() const { return entry_; }
    void setEntry(const Entry& entry);  // brings back an earlier input, templates and all
    void insert(const QString& piece);
    void typeText(const QString& text);  // as the keyboard types it, one character after another (one undo step)
    // Opens a template; `fill`, if any, is typed into its box and the cursor leaves it (x², x⁻¹).
    void insertTemplate(Template kind, const QString& fill = {});
    bool up();    // inside a fraction, to its numerator; false elsewhere (the history's turn)
    bool down();
    void backspace();
    void left();
    void right();
    void setInput(const QString& text);
    QString selectedText() const { return entry_.selectedText(); }
    bool hasSelection() const { return entry_.hasSelection(); }
    void selectAll();
    QRectF caretRectAt(const Position& p) const;  // where the cursor would be drawn at `p`
    void cut();
    void copy();  // the selection as text; without one, copyRequested (the result's turn)
    // Pastes the first non-empty line of `text`, read into templates.
    void pasteText(const QString& text);
    QMenu* editMenu() const { return editMenu_; }  // undo, redo, cut, copy, paste, select all
    // Just after a result: the next digit, name, ( or template starts a new expression, and the next
    // operator goes on from Ans. Moving the cursor or deleting edits the expression instead.
    void setFresh(bool fresh) { fresh_ = fresh; }
    bool fresh() const { return fresh_; }
    void finishName();  // ends a name typed just before the cursor (pi becomes π)
    // While completions are offered, ▲ ▼ Tab Enter Esc are theirs: emitted as completionKey, not handled here.
    void setCompleting(bool completing) { completing_ = completing; }
    void complete(const QString& name);  // replaces the name being typed with `name`
    // The phone's on-screen keyboard (an input method): off in the browser, where it would cover the
    // keypad, until asked for. A physical keyboard types either way.
    void setSystemKeyboard(bool on);
    void clear();  // the input and the result, like AC

    void showValue(const view::ValueParts& parts);
    void showExact(const view::FractionParts& parts);
    void showText(const QString& text);  // a result converted with "to": plain text
    void clearResult();  // the input stays
    QString outputText() const;  // the result on one line of plain text
    // A result worked out while the expression is still being typed: drawn smaller until = confirms it.
    void setProvisional(bool provisional);
    bool provisional() const { return provisional_; }
    QSize resultSize() const;  // the laid-out result, rounded up
    // Underlines the items that bytes [begin, end) of input() come from (an error's span), until the next edit.
    void setMarked(int begin, int end);
    void clearMarked();
    QString markedText() const { return marked_.selectedText(); }
    // The status line, as on the calculator: M while the memory holds something.
    void setMemory(const QString& memory);  // empty when cleared; its value is the screen's tooltip
    QString memory() const { return memory_; }
    QString statusText() const;  // the marks in the corner: "STO" while storing, "M" while the memory holds something
    void setStoring(bool on);
    // How the input is read, "(2 ^ (3 ^ 2))": drawn small and dimmed under the input while the result is a preview.
    void setReading(const QString& reading);
    QString readingText() const { return reading_; }

    static QString fontFamily();  // the bundled screen typeface
    QColor background() const;    // an LCD panel: pale grey-green, or dark in a dark theme
    QColor ink() const;
    QColor noiseColor() const;    // halfway between the ink and the panel
    QSize sizeHint() const override;

    // Small controls along the screen's bottom edge (Details, Cancel…), on the left or the right.
    void addToBar(QWidget* widget, bool right = false);

public slots:
    void undo();
    void redo();
    // Paste from the clipboard, for buttons and menus. In the browser the clipboard is read
    // asynchronously, and the browser may refuse.
    void requestPaste();

signals:
    void evaluateRequested();
    void inputChanged();  // edited by the user, undo and redo included (not by setInput or setEntry)
    // After each edit or movement: the name being typed at the cursor, or "" when none is (or the cursor moved).
    void nameTyped(const QString& name);
    void completionKey(int key);
    void historyRequested(int step);  // +1 for an older entry (▲, Page Up), −1 for a newer one (▼, Page Down)
    void copyRequested();
    void copyMenuRequested();  // Ctrl+Shift+C: every form of the result
    void pastedFirstLine(int lines);  // of `lines` non-empty ones
    void pasteRefused();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool focusNextPrevChild(bool next) override;  // false: Tab stays in the screen

private:
    enum class Shown { Nothing, Value, Exact, Text };

    bool dark() const;
    QFont statusFont() const;
    QFont inputFont() const;
    QFont outputFont() const;
    typeset::Box inputBox(QRectF* caret = nullptr) const;
    QPointF inputOrigin(const typeset::Box& input, const QRectF& caret) const;  // where the input is drawn
    Position positionAt(QPointF point) const;  // the place for the cursor nearest to a point of the widget
    QRectF resultArea(const typeset::Box& input) const;
    int barHeight() const;
    void changed();  // lays the result out again and repaints
    void buildEditMenu();
    void startEditing(bool needsLeftOperand);  // the first edit after a result
    void announceResult();  // to screen readers
    void retranslate();
    // Every change of the input goes through here, so that it can be undone (when it changed anything).
    void edit(const std::function<void()>& change, bool byUser = true);

    Entry entry_;
    UndoStack undo_;
    QMenu* editMenu_ = nullptr;
    bool fresh_ = false;
    bool completing_ = false;
    bool provisional_ = false;
    Entry marked_;  // a copy of the input whose selection is the marked part; none when nothing is marked
    QString reading_;
    bool storing_ = false;
    QString text_;  // Shown::Text: a conversion
    Shown shown_ = Shown::Nothing;
    view::ValueParts value_;
    view::FractionParts exact_;
    QString memory_;
    typeset::Box result_;  // laid out once per change, not on every repaint
    QScrollBar* scroll_ = nullptr;  // for results taller than the screen; nothing is truncated
    QWidget* bar_ = nullptr;
    QHBoxLayout* barLayout_ = nullptr;
    int leftOfBar_ = 0;  // widgets before the bar's stretch
    Position dragFrom_;  // where a drag that selects began
    bool dragging_ = false;
};
