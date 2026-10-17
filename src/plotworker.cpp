#include "plotworker.hpp"

#include "plotfonts.hpp"

#include <calculate-plot/calculate-plot.hpp>

#include <mutex>

void PlotWorker::draw(int serial, const QByteArray& figure, QSizeF size, double ratio) {
    if (serial != newest_.load()) return;  // a newer request is queued behind this one
    static std::once_flag fonts;
    std::call_once(fonts, installPlotFonts);
    std::string error;
    const calculate_plot::Image drawn = calculate_plot::drawCanvas(std::string_view(figure.constData(), static_cast<std::size_t>(figure.size())),
                                                                   calculate_plot::Canvas{size.width(), size.height(), ratio}, std::string(), &error);
    if (!error.empty()) {
        emit this->drawn(serial, QImage(), QString::fromStdString(error));
        return;
    }
    QImage image(drawn.width, drawn.height, QImage::Format_RGBA8888);
    for (int y = 0; y < drawn.height; ++y)
        std::copy_n(drawn.rgba.data() + std::size_t(y) * std::size_t(drawn.width) * 4, std::size_t(drawn.width) * 4, image.scanLine(y));
    image.setDevicePixelRatio(ratio);
    emit this->drawn(serial, image, QString());
}
