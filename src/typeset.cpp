#include "typeset.hpp"

#include <QFontMetricsF>

namespace typeset {

namespace {

// Adds `part` to `box`, moved by (dx, dy).
void place(Box& box, const Box& part, qreal dx, qreal dy) {
    for (Run run : part.runs) {
        run.origin += QPointF(dx, dy);
        box.runs << run;
    }
    for (const QLineF& line : part.lines) box.lines << line.translated(dx, dy);
}

qreal advance(const Run& run) { return QFontMetricsF(run.font).horizontalAdvance(run.text); }

QFont smaller(const QFont& font) {
    QFont f = font;
    f.setPixelSize(qMax(1, qRound(QFontMetricsF(font).height() * 0.6)));
    return f;
}

}  // namespace

QPointF end(const Box& box) {
    QPointF point;
    for (const Run& run : box.runs)
        if (run.origin.y() >= point.y()) point = QPointF(run.origin.x() + advance(run), run.origin.y());
    return point;
}

Box text(const QString& s, const QFont& font, Role role) {
    const QFontMetricsF m(font);
    Box b;
    b.runs << Run{s, QPointF(0, 0), font, role};
    b.width = m.horizontalAdvance(s);
    b.ascent = m.ascent();
    b.descent = m.descent();
    return b;
}

Box row(const QList<Box>& boxes) {
    Box b;
    for (const Box& part : boxes) {
        place(b, part, b.width, 0);
        b.width += part.width;
        b.ascent = qMax(b.ascent, part.ascent);
        b.descent = qMax(b.descent, part.descent);
    }
    return b;
}

// The bar sits on the maths axis (the height of a minus sign), the numerator just above it and the
// denominator just below, both centred.
Box fraction(const Box& numerator, const Box& denominator, const QFont& font) {
    const QFontMetricsF m(font);
    const qreal axis = -m.strikeOutPos();
    const qreal gap = m.height() * 0.1;
    const qreal pad = m.horizontalAdvance(' ') * 0.25;
    Box b;
    b.width = qMax(numerator.width, denominator.width) + 2 * pad;
    b.lines << QLineF(0, axis, b.width, axis);
    place(b, numerator, (b.width - numerator.width) / 2, axis - gap - numerator.descent);
    place(b, denominator, (b.width - denominator.width) / 2, axis + gap + denominator.ascent);
    b.ascent = -axis + gap + numerator.descent + numerator.ascent;
    b.descent = axis + gap + denominator.ascent + denominator.descent;
    return b;
}

Box superscript(const Box& base, const Box& exponent) {
    const qreal raise = base.ascent * 0.5;
    Box b;
    place(b, base, 0, 0);
    place(b, exponent, base.width, -raise);
    b.width = base.width + exponent.width;
    b.ascent = qMax(base.ascent, raise + exponent.ascent);
    b.descent = qMax(base.descent, exponent.descent - raise);
    return b;
}

Box overline(const Box& box) {
    const qreal gap = box.ascent * 0.08;
    Box b = box;
    b.lines << QLineF(0, -box.ascent - gap, box.width, -box.ascent - gap);
    b.ascent += 2 * gap;
    return b;
}

Box paragraph(const QList<Segment>& segments, const QFont& font, qreal maxWidth) {
    const QFontMetricsF m(font);
    Box b;
    b.ascent = m.ascent();
    QPointF pen;
    QString pending;  // characters of the current role on the current line, not yet a run
    qreal pendingWidth = 0;
    Role role = Role::Plain;
    const auto flush = [&] {
        if (pending.isEmpty()) return;
        b.runs << Run{pending, pen, font, role};
        pen.rx() += pendingWidth;
        b.width = qMax(b.width, pen.x());
        pending.clear();
        pendingWidth = 0;
    };
    for (const Segment& segment : segments) {
        flush();
        role = segment.role;
        for (const QChar c : segment.text) {
            const qreal w = m.horizontalAdvance(c);
            if (maxWidth > 0 && pen.x() + pendingWidth + w > maxWidth + 0.001 && (pen.x() > 0 || !pending.isEmpty())) {
                flush();
                pen = QPointF(0, pen.y() + m.lineSpacing());
            }
            pending += c;
            pendingWidth += w;
        }
    }
    flush();
    b.descent = pen.y() + m.descent();
    return b;
}

Box value(const view::ValueParts& parts, const QFont& font, qreal maxWidth) {
    QList<Segment> segments{{parts.trusted, Role::Plain}};
    if (!parts.noise.isEmpty()) segments << Segment{QStringLiteral("|") + parts.noise, Role::Noise};
    Box b = paragraph(segments, font, maxWidth);
    if (parts.exponent.isEmpty()) return b;
    const Box tail = superscript(text(QStringLiteral("×10"), font), text(parts.exponent, smaller(font)));
    QPointF at = end(b);
    if (maxWidth > 0 && at.x() + tail.width > maxWidth) at = QPointF(0, at.y() + QFontMetricsF(font).lineSpacing());
    place(b, tail, at.x(), at.y());
    b.width = qMax(b.width, at.x() + tail.width);
    b.ascent = qMax(b.ascent, tail.ascent - at.y());
    b.descent = qMax(b.descent, at.y() + tail.descent);
    return b;
}

// The fraction, then " = " and its decimal with the recurring digits overlined; a whole number alone.
// The decimal moves to its own line when the whole does not fit.
Box exact(const view::FractionParts& parts, const QFont& font, qreal maxWidth) {
    if (parts.denominator == QStringLiteral("1")) return paragraph({{parts.sign + parts.numerator}}, font, maxWidth);
    Box result = fraction(paragraph({{parts.numerator}}, font, maxWidth), paragraph({{parts.denominator}}, font, maxWidth), font);
    if (!parts.sign.isEmpty()) result = row({text(parts.sign, font), result});
    if (parts.decimal.isEmpty()) return result;
    QList<Box> decimal{text(QStringLiteral(" = "), font), text(parts.sign + parts.decimal, font)};
    if (!parts.recurring.isEmpty()) decimal << overline(text(parts.recurring, font));
    const Box tail = row(decimal);
    if (maxWidth <= 0 || result.width + tail.width <= maxWidth) return row({result, tail});
    Box b = result;
    const qreal y = result.descent + QFontMetricsF(font).lineSpacing();
    place(b, tail, 0, y);
    b.width = qMax(result.width, tail.width);
    b.descent = y + tail.descent;
    return b;
}

}  // namespace typeset
