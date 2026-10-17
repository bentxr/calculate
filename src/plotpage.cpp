#include "plotpage.hpp"

#include "filedialogs.hpp"
#include "plotcanvas.hpp"
#include "ploteditor.hpp"
#include "plotfonts.hpp"

#include "icons.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QAction>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QShortcut>
#include <QToolButton>

namespace {

std::string text(const QByteArray& b) { return std::string(b.constData(), std::size_t(b.size())); }

}  // namespace

PlotPage::PlotPage(QWidget* parent) : QWidget(parent) {
    setObjectName("plotPage");
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    // The canvas' toolbar: matplotlib's navigation (Home, Back, Forward, Pan, Zoom), Open and Save, and the pointer's
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
    QToolButton* openButton = button("plotOpen", icons::Kind::Open, false);
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
    // Save: the figure as savefig writes a PNG or an SVG, as a Python script, or the figure file itself (D9.9)
    auto* saveMenu = new QMenu(saveButton);
    saveMenu->setObjectName("plotSaveMenu");
    const auto format = [this, saveMenu](const char* name, const QString& file, std::function<QByteArray(QString*)> bytes) {
        QAction* a = saveMenu->addAction(QString());
        a->setObjectName(name);
        connect(a, &QAction::triggered, this, [this, file, bytes] {
            ensurePlotFonts();
            QString error;
            const QByteArray content = bytes(&error);
            if (!error.isEmpty()) return coordinates_->setText(error);
            filedialogs::save(content, file, this);
        });
    };
    format("save:png", QStringLiteral("figure.png"), [this](QString* why) { return canvas_->pngBytes(why); });
    format("save:svg", QStringLiteral("figure.svg"), [this](QString* why) {
        std::string error;
        const std::string svg = calculate_plot::drawSvg(text(canvas_->figure()), std::string(), &error);
        *why = QString::fromStdString(error);
        return QByteArray::fromStdString(svg);
    });
    format("save:py", QStringLiteral("figure.py"), [this](QString* why) {
        std::string error;
        const std::string py = calculate_plot::exportPython(text(canvas_->figure()), &error);
        *why = QString::fromStdString(error);
        return QByteArray::fromStdString(py);
    });
    format("save:figure", QStringLiteral("figure.json"), [this](QString*) { return canvas_->figure(); });
    saveButton->setMenu(saveMenu);
    saveButton->setPopupMode(QToolButton::InstantPopup);
    connect(canvas_, &PlotCanvas::saveRequested, this,
            [saveButton, saveMenu] { saveMenu->popup(saveButton->mapToGlobal(QPoint(0, saveButton->height()))); });
    // Open: a figure file becomes the document; one the library refuses is not taken (with why, by the file's name)
    const auto open = [this] {
        filedialogs::open(
            tr("Figure files (*.json)"),
            [this](const QString& name, const QByteArray& content) {
                if (name.isEmpty()) return;  // cancelled
                const std::string refused = calculate_plot::checkFigure(text(content), std::string());
                if (!refused.empty()) return coordinates_->setText(tr("%1 was not opened: %2").arg(name, QString::fromStdString(refused)));
                canvas_->setFigure(content);
                editor_->setFigure(content);
            },
            this);
    };
    connect(openButton, &QToolButton::clicked, this, open);
    auto* openKey = new QShortcut(QKeySequence::Open, this, open);
    openKey->setContext(Qt::WidgetWithChildrenShortcut);
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
        {"plotOpen", tr("Open a figure file")},
        {"plotSave", tr("Save the figure: as an image, a Python script or a figure file")},
    };
    for (const auto& [name, tip] : tips) {
        auto* b = findChild<QToolButton*>(QString::fromLatin1(name));
        b->setToolTip(tip);
        b->setAccessibleName(tip.section(QLatin1Char(':'), 0, 0));
    }
    const std::pair<const char*, QString> formats[] = {
        {"save:png", tr("PNG image")}, {"save:svg", tr("SVG image")}, {"save:py", tr("Python script")}, {"save:figure", tr("Figure file")}};
    for (const auto& [name, label] : formats) findChild<QAction*>(QString::fromLatin1(name))->setText(label);
}
