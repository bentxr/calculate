#include "plotworker.hpp"

#include "plotfonts.hpp"

namespace {

std::string_view view(const QByteArray& bytes) { return std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())); }

QImage toImage(const calculate_plot::Image& drawn, double ratio) {
    QImage image(drawn.width, drawn.height, QImage::Format_RGBA8888);
    for (int y = 0; y < drawn.height; ++y)
        std::copy_n(drawn.rgba.data() + std::size_t(y) * std::size_t(drawn.width) * 4, std::size_t(drawn.width) * 4, image.scanLine(y));
    image.setDevicePixelRatio(ratio);
    return image;
}

}  // namespace

void PlotWorker::draw(int serial, const QByteArray& figure, QSizeF size, double ratio) {
    if (serial != newest_.load()) return;  // a newer request is queued behind this one
    ensurePlotFonts();
    std::string error;
    const calculate_plot::Image drawn = calculate_plot::drawCanvas(view(figure), calculate_plot::Canvas{size.width(), size.height(), ratio}, std::string(), &error);
    if (!error.empty()) return emit this->drawn(serial, QImage(), QString::fromStdString(error));
    emit this->drawn(serial, toImage(drawn, ratio), QString());
}

void PlotWorker::navigate(int serial, const QByteArray& figure, QSizeF size, double ratio, const calculate_plot::Gesture& gesture, bool commit) {
    if (!commit && serial != newest_.load()) return;
    ensurePlotFonts();
    std::string error;
    const calculate_plot::Canvas canvas{size.width(), size.height(), ratio};
    const QByteArray after = QByteArray::fromStdString(calculate_plot::navigate(view(figure), canvas, gesture, std::string(), &error));
    if (!error.empty()) return emit navigated(serial, figure, QImage(), QString::fromStdString(error), commit);
    const calculate_plot::Image drawn = calculate_plot::drawCanvas(view(after), canvas, std::string(), &error);
    if (!error.empty()) return emit navigated(serial, figure, QImage(), QString::fromStdString(error), commit);
    emit navigated(serial, after, toImage(drawn, ratio), QString(), commit);
}

void PlotWorker::describe(int serial, const QByteArray& figure, QSizeF size, double ratio, QPointF at) {
    if (serial != newestText_.load()) return;
    ensurePlotFonts();
    std::string error;
    const auto texts = calculate_plot::pointerTexts(view(figure), calculate_plot::Canvas{size.width(), size.height(), ratio}, {{at.x(), at.y()}},
                                                    std::string(), &error);
    emit described(serial, texts.empty() ? QString() : QString::fromStdString(texts[0]));
}
