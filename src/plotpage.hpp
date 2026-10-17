#pragma once

#include <QWidget>

#include "icons.hpp"

#include <QList>
#include <utility>

class PlotCanvas;
class PlotEditor;
class QHBoxLayout;
class QLabel;
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

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawIcons();  // in the theme's text colour
    // The phone's editor sheet: opened with a tab (or the search) in front, closed, placed over the canvas
    void openSheet(void (PlotEditor::*show)());
    void closeSheet();
    void placeSheet();

    QList<std::pair<QToolButton*, icons::Kind>> toolButtons_;
    QWidget* tools_ = nullptr;
    QLabel* coordinates_ = nullptr;
    QWidget* contextBar_ = nullptr;
    QHBoxLayout* context_ = nullptr;
    QToolButton* details_ = nullptr;
    PlotCanvas* canvas_ = nullptr;
    PlotEditor* editor_ = nullptr;
    QWidget* sheet_ = nullptr;
    QWidget* sheetBar_ = nullptr;
    bool narrow_ = false;
};
