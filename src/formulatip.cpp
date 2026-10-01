#include "formulatip.hpp"

#include "lcd.hpp"
#include "presenter.hpp"

#include <QFontInfo>
#include <QPainter>

namespace {

constexpr int pad = 8;

// One box under the other, left-aligned.
typeset::Box stacked(const typeset::Box& top, const typeset::Box& bottom, qreal gap) {
    typeset::Box b = top;
    const qreal y = top.descent + gap + bottom.ascent;
    for (typeset::Run run : bottom.runs) {
        run.origin.ry() += y;
        b.runs << run;
    }
    for (const QLineF& line : bottom.lines) b.lines << line.translated(0, y);
    b.width = qMax(top.width, bottom.width);
    b.descent = y + bottom.descent;
    return b;
}

// The formulas the engine evaluates (parser.cpp, statistic(); functions.hpp, Median).
typeset::Box formula(const QString& function, const QFont& font, const QString& odd, const QString& even) {
    using namespace typeset;
    QFont small = font;
    small.setPixelSize(qRound(QFontInfo(font).pixelSize() * 0.7));
    const auto t = [&](const QString& s) { return text(s, font); };
    const auto x = [&](const QString& i) { return subscript(t("x"), text(i, small)); };
    const auto squared = [&](const QString& s) { return superscript(t(s), text("2", small)); };
    const Box sum = bigOperator(QStringLiteral("Σ"), text("i=1", small), text("n", small), font);
    const Box mean = overline(t("x"));
    const Box deviations = row({sum, t("("), x("i"), t(" − "), mean, squared(")")});
    if (function == "mean") return row({mean, t(" = "), fraction(row({sum, x("i")}), t("n"), font)});
    if (function == "var") return row({squared("s"), t(" = "), fraction(deviations, t("n − 1"), font)});
    if (function == "varp") return row({squared("σ"), t(" = "), fraction(deviations, t("n"), font)});
    if (function == "stdev") return row({t("s = "), radical(fraction(deviations, t("n − 1"), font), font)});
    if (function == "stdevp") return row({t("σ = "), radical(fraction(deviations, t("n"), font), font)});
    if (function == "median")  // over the values sorted from the smallest
        return stacked(row({t("x̃ = "), x("(n+1)/2"), text("   " + odd, small)}),
                       row({t("x̃ = "), fraction(row({x("n/2"), t(" + "), x("n/2+1")}), t("2"), font), text("   " + even, small)}),
                       QFontMetricsF(font).height() * 0.4);
    return {};
}

}  // namespace

FormulaTip::FormulaTip(QWidget* parent) : QWidget(parent, Qt::ToolTip) {}

QString FormulaTip::algorithm() const { return view::algorithm(function_); }

void FormulaTip::showFor(const QString& function, const QPoint& at) {
    function_ = function;
    QFont math(Lcd::fontFamily());
    math.setPixelSize(qRound(QFontInfo(font()).pixelSize() * 1.3));
    formula_ = formula(function, math, tr("n odd"), tr("n even"));
    const QRect words = textRect();
    resize(qCeil(qMax(formula_.width, qreal(words.width()))) + 2 * pad, words.bottom() + pad);
    move(at + QPoint(12, 16));
    show();
    update();
}

QRect FormulaTip::textRect() const {
    const int width = qMax(qCeil(formula_.width), fontMetrics().averageCharWidth() * 40);
    const QRect bounds = fontMetrics().boundingRect(QRect(0, 0, width, 10000), Qt::TextWordWrap, algorithm());
    return bounds.translated(pad, pad + qCeil(formulaHeight()) + pad);
}

void FormulaTip::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().color(QPalette::ToolTipBase));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    const QColor ink = palette().color(QPalette::ToolTipText);
    typeset::paint(painter, formula_, QPointF(pad, pad + formula_.ascent), ink, ink, rect());
    painter.setFont(font());
    painter.setPen(ink);
    painter.drawText(textRect(), Qt::TextWordWrap, algorithm());
}
