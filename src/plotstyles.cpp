#include "plotstyles.hpp"

#include "plotfonts.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <QGridLayout>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

PlotStylesDialog::PlotStylesDialog(const QByteArray& figure, QWidget* parent) : QDialog(parent), figure_(figure) {
    setObjectName("plotStyles");
    names_ << QStringLiteral("default");
    for (const std::string& name : calculate_plot::styleNames()) names_ << QString::fromStdString(name);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* tiles = new QWidget;
    auto* grid = new QGridLayout(tiles);
    const int columns = 3;
    for (int i = 0; i < names_.size(); ++i) {
        auto* tile = new QToolButton(tiles);
        tile->setObjectName("style:" + names_[i]);
        tile->setText(names_[i]);
        tile->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        tile->setIconSize(thumbnailSize());
        tile->setAutoRaise(true);
        const QString name = names_[i];
        connect(tile, &QToolButton::clicked, this, [this, name] {
            chosen_ = name;
            accept();
        });
        grid->addWidget(tile, i / columns, i % columns);
    }
    scroll->setWidget(tiles);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scroll);
    // within the parent window: the whole of it on a phone
    if (parent) {
        const QRect area = parent->window()->geometry();
        const QSize size = area.width() < area.height() ? area.size() : area.size().boundedTo(QSize(620, 640));
        setGeometry(QRect(area.center() - QPoint(size.width() / 2, size.height() / 2), size) & area);
    }
    retranslate();
    QTimer::singleShot(0, this, &PlotStylesDialog::drawNext);
}

void PlotStylesDialog::drawNext() {
    if (finished()) return;
    const QString name = names_[next_++];
    ensurePlotFonts();
    std::string error;
    const std::string style = name == "default" ? std::string() : "\"" + name.toStdString() + "\"";
    const std::string styled = calculate_plot::setStyle(figure_.toStdString(), style, &error);
    // a miniature: the figure as a 640 x 480 canvas shows it, drawn at a quarter of the screen's ratio, so the tile
    // holds all of it with the text in its place (a figure laid out at the tile's size would squeeze it)
    const double ratio = devicePixelRatioF();
    const calculate_plot::Canvas canvas{4.0 * thumbnailSize().width(), 4.0 * thumbnailSize().height(), 0.25 * ratio};
    const calculate_plot::Image drawn = error.empty() ? calculate_plot::drawCanvas(styled, canvas, std::string(), &error) : calculate_plot::Image{};
    if (error.empty() && drawn.width > 0) {
        QImage image(drawn.width, drawn.height, QImage::Format_RGBA8888);
        for (int y = 0; y < drawn.height; ++y)
            std::copy_n(drawn.rgba.data() + std::size_t(y) * std::size_t(drawn.width) * 4, std::size_t(drawn.width) * 4, image.scanLine(y));
        image.setDevicePixelRatio(ratio);
        thumbnails_.insert(name, image);
        if (auto* tile = findChild<QToolButton*>("style:" + name)) tile->setIcon(QIcon(QPixmap::fromImage(image)));
    }
    QTimer::singleShot(0, this, &PlotStylesDialog::drawNext);
}

void PlotStylesDialog::retranslate() { setWindowTitle(tr("Styles")); }
