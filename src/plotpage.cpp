#include "plotpage.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QToolButton>

PlotPage::PlotPage(QWidget* parent) : QWidget(parent) {
    setObjectName("plotPage");
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    auto* tools = new QWidget(this);  // the canvas' toolbar (navigation, export, undo, open)
    tools->setObjectName("plotTools");
    contextBar_ = new QWidget(this);
    contextBar_->setObjectName("plotContext");
    context_ = new QHBoxLayout(contextBar_);
    context_->setContentsMargins(0, 0, 0, 0);
    details_ = new QToolButton(contextBar_);
    details_->setObjectName("plotDetails");
    details_->setEnabled(false);  // nothing computed yet
    context_->addWidget(details_);
    auto* canvas = new QWidget(this);
    canvas->setObjectName("plotCanvas");
    editor_ = new QWidget(this);
    editor_->setObjectName("plotEditor");
    editor_->setFixedWidth(340);
    grid->addWidget(tools, 0, 0);
    grid->addWidget(contextBar_, 0, 1);
    grid->addWidget(canvas, 1, 0);
    grid->addWidget(editor_, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setRowStretch(1, 1);
    details_->hide();  // shown by the window while Plot is the mode
    retranslate();
}

void PlotPage::setNarrow(bool narrow) {
    contextBar_->setVisible(!narrow);
    editor_->setVisible(!narrow);
}

void PlotPage::retranslate() { details_->setText(tr("Details")); }
