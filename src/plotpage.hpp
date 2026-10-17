#pragma once

#include <QWidget>

class PlotCanvas;
class QHBoxLayout;
class QToolButton;

// The Plot section's page. Wide: a line across the top (the canvas' tools, then the calculation's context: the angle
// unit, the number type and Details, over the editor) and under it the canvas beside the editor. The window lends it
// its angle and type widgets while Plot is the mode, so they stay one setting with the other modes.
class PlotPage : public QWidget {
    Q_OBJECT

public:
    explicit PlotPage(QWidget* parent = nullptr);

    // Where the window puts the angle and type (wide), followed by details().
    QHBoxLayout* context() const { return context_; }
    // Details of the plotted series' calculations: their errors and cautions. The window places it (wide: in the
    // context bar; phone: in its top row, where = was).
    QToolButton* details() const { return details_; }
    PlotCanvas* canvas() const { return canvas_; }
    // The phone's arrangement has no context bar and no editor beside the canvas.
    void setNarrow(bool narrow);
    void retranslate();

private:
    QWidget* contextBar_ = nullptr;
    QHBoxLayout* context_ = nullptr;
    QToolButton* details_ = nullptr;
    PlotCanvas* canvas_ = nullptr;
    QWidget* editor_ = nullptr;
};
