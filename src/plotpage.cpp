#include "plotpage.hpp"

#include "plotcanvas.hpp"
#include "ploteditor.hpp"

#include "icons.hpp"

#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>

PlotPage::PlotPage(QWidget* parent) : QWidget(parent) {
    setObjectName("plotPage");
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    // The canvas' toolbar: matplotlib's navigation (Home, Back, Forward, Pan, Zoom, Save) and the pointer's
    // coordinates
    auto* tools = new QWidget(this);
    tools->setObjectName("plotTools");
    auto* toolRow = new QHBoxLayout(tools);
    toolRow->setContentsMargins(0, 0, 0, 0);
    const auto button = [this, tools, toolRow](const char* name, icons::Kind kind, bool checkable) {
        auto* b = new QToolButton(tools);
        b->setObjectName(name);
        b->setAutoRaise(true);
        b->setCheckable(checkable);
        b->setFocusPolicy(Qt::NoFocus);  // the keys stay with the canvas
        toolRow->addWidget(b);
        tools_.append({b, kind});
        return b;
    };
    QToolButton* homeButton = button("plotHome", icons::Kind::Home, false);
    QToolButton* backButton = button("plotBack", icons::Kind::Back, false);
    QToolButton* forwardButton = button("plotForward", icons::Kind::Forward, false);
    toolRow->addSpacing(8);
    QToolButton* panButton = button("plotPan", icons::Kind::Pan, true);
    QToolButton* zoomButton = button("plotZoom", icons::Kind::Zoom, true);
    toolRow->addSpacing(8);
    QToolButton* saveButton = button("plotSave", icons::Kind::Save, false);
    toolRow->addStretch(1);
    coordinates_ = new QLabel(tools);
    coordinates_->setObjectName("plotCoordinates");
    coordinates_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);  // a long text is cut, never widens the page
    coordinates_->setAlignment(Qt::AlignCenter);
    toolRow->addWidget(coordinates_);
    toolRow->addStretch(1);
    contextBar_ = new QWidget(this);
    contextBar_->setObjectName("plotContext");
    context_ = new QHBoxLayout(contextBar_);
    context_->setContentsMargins(0, 0, 0, 0);
    details_ = new QToolButton(contextBar_);
    details_->setObjectName("plotDetails");
    details_->setEnabled(false);  // nothing computed yet
    context_->addWidget(details_);
    // A new document: one empty Axes in matplotlib's default look (D9.2: the figure file is the document).
    canvas_ = new PlotCanvas(this);
    canvas_->setFigure(R"({"version": 1, "calls": [{"call": "subplots", "as": ["fig", "ax"]}]})");
    connect(homeButton, &QToolButton::clicked, canvas_, &PlotCanvas::home);
    connect(backButton, &QToolButton::clicked, canvas_, &PlotCanvas::back);
    connect(forwardButton, &QToolButton::clicked, canvas_, &PlotCanvas::forward);
    connect(panButton, &QToolButton::clicked, canvas_, [this] { canvas_->setTool(PlotCanvas::Tool::Pan); });
    connect(zoomButton, &QToolButton::clicked, canvas_, [this] { canvas_->setTool(PlotCanvas::Tool::Zoom); });
    connect(canvas_, &PlotCanvas::toolChanged, this, [panButton, zoomButton](PlotCanvas::Tool tool) {
        panButton->setChecked(tool == PlotCanvas::Tool::Pan);
        zoomButton->setChecked(tool == PlotCanvas::Tool::Zoom);
    });
    const auto views = [this, backButton, forwardButton] {
        backButton->setEnabled(canvas_->canGoBack());
        forwardButton->setEnabled(canvas_->canGoForward());
    };
    connect(canvas_, &PlotCanvas::viewsChanged, this, views);
    views();
    connect(canvas_, &PlotCanvas::coordinatesChanged, coordinates_, &QLabel::setText);
    const auto save = [this] { QFileDialog::saveFileContent(canvas_->pngBytes(), QStringLiteral("figure.png"), this); };
    connect(saveButton, &QToolButton::clicked, this, save);
    connect(canvas_, &PlotCanvas::saveRequested, this, save);
    auto* editor = new PlotEditor(this);
    editor->setFixedWidth(340);
    editor_ = editor;
    editor->setFigure(canvas_->figure());
    connect(editor, &PlotEditor::figureEdited, canvas_, &PlotCanvas::setFigure);  // an edit is the new document
    connect(canvas_, &PlotCanvas::viewsChanged, editor, [this, editor] { editor->setFigure(canvas_->figure()); });
    grid->addWidget(tools, 0, 0);
    grid->addWidget(contextBar_, 0, 1);
    grid->addWidget(canvas_, 1, 0);
    grid->addWidget(editor_, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setRowStretch(1, 1);
    details_->hide();  // shown by the window while Plot is the mode
    drawIcons();
    retranslate();
}

void PlotPage::drawIcons() {
    const QColor ink = palette().color(QPalette::ButtonText);
    for (const auto& [b, kind] : tools_) b->setIcon(icons::drawn(kind, ink, 20));
}

void PlotPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) drawIcons();
    if (event->type() == QEvent::LanguageChange) retranslate();
    QWidget::changeEvent(event);
}

void PlotPage::setNarrow(bool narrow) {
    contextBar_->setVisible(!narrow);
    editor_->setVisible(!narrow);
}

void PlotPage::retranslate() {
    details_->setText(tr("Details"));
    const std::pair<const char*, QString> tips[] = {
        {"plotHome", tr("Home: the first view")},
        {"plotBack", tr("Back: the previous view")},
        {"plotForward", tr("Forward: the next view")},
        {"plotPan", tr("Pan: the left button moves the view, the right button zooms it")},
        {"plotZoom", tr("Zoom to a rectangle: the left button zooms in, the right button out")},
        {"plotSave", tr("Save the figure as PNG")},
    };
    for (const auto& [name, tip] : tips) {
        auto* b = findChild<QToolButton*>(QString::fromLatin1(name));
        b->setToolTip(tip);
        b->setAccessibleName(tip.section(QLatin1Char(':'), 0, 0));
    }
}
