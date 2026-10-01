#pragma once

#include "entry.hpp"
#include "presenter.hpp"
#include "typeset.hpp"

#include <QWidget>

class QHBoxLayout;
class QScrollBar;

// The calculator's screen: the input at the top and the result at the bottom right, drawn in two
// dimensions. It owns the keyboard, and lets through only what the calculator's keys could type:
// digits, the decimal point, + − × ÷, ( ), Enter, Backspace, Esc and the arrows.
class Lcd : public QWidget {
    Q_OBJECT

public:
    explicit Lcd(QWidget* parent = nullptr);

    QString input() const { return entry_.text(); }
    void insert(const QString& piece);
    void backspace();
    void left();
    void right();
    void setInput(const QString& text);
    void clear();  // the input and the result, like AC

    void showValue(const view::ValueParts& parts);
    void showExact(const view::FractionParts& parts);
    void showMessage(const QString& message);
    QString outputText() const;  // the result on one line of plain text
    // The status line, as on the calculator: S (SHIFT), A (ALPHA), M (memory in use).
    void setStatus(bool shift, bool alpha);
    void setMemory(const QString& memory);  // empty when cleared; its value is the screen's tooltip
    QString memory() const { return memory_; }
    QString statusText() const;

    static QString fontFamily();  // the bundled screen typeface
    QColor background() const;    // an LCD panel: pale grey-green, or dark in a dark theme
    QColor ink() const;
    QColor noiseColor() const;    // halfway between the ink and the panel
    QSize sizeHint() const override;

    // Small controls along the screen's bottom edge (Details, Cancel…), on the left or the right.
    void addToBar(QWidget* widget, bool right = false);

signals:
    void evaluateRequested();
    void historyRequested(int step);  // +1 for an older entry (▲), −1 for a newer one (▼)

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class Shown { Nothing, Value, Exact, Message };

    bool dark() const;
    QFont statusFont() const;
    QFont inputFont() const;
    QFont outputFont() const;
    typeset::Box inputBox() const;
    QRectF resultArea(const typeset::Box& input) const;
    int barHeight() const;
    void changed();  // lays the result out again and repaints

    Entry entry_;
    Shown shown_ = Shown::Nothing;
    view::ValueParts value_;
    view::FractionParts exact_;
    QString message_;
    bool shift_ = false;
    bool alpha_ = false;
    QString memory_;
    typeset::Box result_;  // laid out once per change, not on every repaint
    QScrollBar* scroll_ = nullptr;  // for results taller than the screen; nothing is truncated
    QWidget* bar_ = nullptr;
    QHBoxLayout* barLayout_ = nullptr;
    int leftOfBar_ = 0;  // widgets before the bar's stretch
};
