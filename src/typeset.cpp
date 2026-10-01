#include "typeset.hpp"

#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>

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

QFont scaled(const QFont& font, qreal factor) {
    QFont f = font;
    f.setPixelSize(qMax(6, qRound(QFontInfo(font).pixelSize() * factor)));
    return f;
}

// Nothing, with a line's height: what an exponent or a lowered base attaches to.
Box strut(const QFont& font) {
    const QFontMetricsF m(font);
    Box b;
    b.ascent = m.ascent();
    b.descent = m.descent();
    return b;
}

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

Box subscript(const Box& base, const Box& index) {
    const qreal lower = base.ascent * 0.3;
    Box b;
    place(b, base, 0, 0);
    place(b, index, base.width, lower);
    b.width = base.width + index.width;
    b.ascent = qMax(base.ascent, index.ascent - lower);
    b.descent = qMax(base.descent, lower + index.descent);
    return b;
}

// The sign is drawn as four strokes: a short rise, the long stroke down, the stroke up to the bar,
// and the bar over the content. The index, if any, sits over the rise.
Box radical(const Box& content, const QFont& font, const Box& index) {
    const QFontMetricsF m(font);
    const qreal tick = m.horizontalAdvance('x') * 0.8;
    const qreal gap = m.height() * 0.08;
    const qreal pad = m.horizontalAdvance(' ') * 0.2;
    const qreal top = -(content.ascent + 2 * gap);
    const qreal bottom = content.descent;
    const qreal x = qMax<qreal>(0, index.width - tick * 0.5);  // room for the index on the left
    const QPointF rise(x, -0.35 * m.ascent()), knee(x + tick * 0.35, -0.45 * m.ascent()),
        foot(x + tick * 0.6, bottom), corner(x + tick, top), end(x + tick + pad + content.width + pad, top);
    Box b;
    if (!index.runs.isEmpty()) {
        const qreal baseline = knee.y() - gap - index.descent;
        place(b, index, knee.x() - index.width, baseline);
        b.ascent = index.ascent - baseline;
    }
    place(b, content, corner.x() + pad, 0);
    b.lines << QLineF(rise, knee) << QLineF(knee, foot) << QLineF(foot, corner) << QLineF(corner, end);
    b.width = end.x();
    b.ascent = qMax(b.ascent, -top + gap);
    b.descent = qMax(content.descent, bottom);
    return b;
}

Box bigOperator(const QString& symbol, const Box& under, const Box& over, const QFont& font) {
    QFont big = font;
    big.setPixelSize(qRound(QFontInfo(font).pixelSize() * 1.5));
    Box sign = text(symbol, big);
    const QRectF ink = QFontMetricsF(big).tightBoundingRect(symbol);  // the glyph itself, not the line
    sign.ascent = -ink.top();
    sign.descent = ink.bottom();
    const qreal gap = QFontMetricsF(font).height() * 0.08;
    const qreal width = qMax(sign.width, qMax(under.width, over.width));
    Box b;
    place(b, sign, (width - sign.width) / 2, 0);
    place(b, under, (width - under.width) / 2, sign.descent + gap + under.ascent);
    place(b, over, (width - over.width) / 2, -(sign.ascent + gap + over.descent));
    b.width = width;
    b.ascent = sign.ascent + gap + over.descent + over.ascent;
    b.descent = sign.descent + gap + under.ascent + under.descent;
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

void paint(QPainter& painter, const Box& box, QPointF origin, const QColor& ink, const QColor& noise, const QRectF& clip) {
    for (const Run& run : box.runs) {
        const QPointF at = origin + run.origin;
        const QFontMetricsF m(run.font);
        if (at.y() + m.descent() < clip.top() || at.y() - m.ascent() > clip.bottom()) continue;
        painter.setFont(run.font);
        painter.setPen(run.role == Role::Noise ? noise : ink);
        painter.drawText(at, run.text);
    }
    painter.setPen(QPen(ink, 1.5));
    for (const QLineF& line : box.lines) painter.drawLine(line.translated(origin));
}

namespace {

// Lays out an entry's rows and templates, marking the cursor with an empty Caret run in its row.
struct InputLayout {
    const Entry& entry;

    Box caret(const QFont& font) const {
        Box b = strut(font);
        b.runs << Run{QString(), QPointF(0, 0), font, Role::Caret};
        return b;
    }

    // `onPath`: the rows that lead to the cursor; the cursor's own row is the last of them.
    Box row(const Row& items, const QFont& font, std::size_t depth, bool onPath) const {
        const bool here = onPath && depth == entry.path().size();
        QList<Box> parts;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (here && static_cast<int>(i) == entry.cursor()) parts << caret(font);
            parts << item(items[i], font, depth, onPath, static_cast<int>(i));
        }
        if (here && entry.cursor() == static_cast<int>(items.size())) parts << caret(font);
        if (items.empty() && depth > 0) parts << text(QStringLiteral("□"), font);  // an empty template box, not the screen
        return typeset::row(parts);
    }

    Box item(const Item& it, const QFont& font, std::size_t depth, bool onPath, int index) const {
        const QFont small = scaled(font, 0.7);
        const auto box = [&](int b, const QFont& f) {
            const bool on = onPath && depth < entry.path().size() && entry.path()[depth] == std::make_pair(index, b);
            return row(it.boxes[static_cast<std::size_t>(b)], f, depth + 1, on);
        };
        switch (it.kind) {
        case Template::Text: return text(it.text, font);
        case Template::Fraction: return fraction(box(0, font), box(1, font), font);
        case Template::Sqrt: return radical(box(0, font), font);
        case Template::Cbrt: return radical(box(0, font), font, text(QStringLiteral("3"), small));
        case Template::Root: return radical(box(1, font), font, box(0, small));
        case Template::Power: return superscript(strut(font), box(0, small));
        case Template::Exp: return superscript(text(QStringLiteral("e"), font), box(0, small));
        case Template::Pow10: return superscript(text(QStringLiteral("10"), font), box(0, small));
        case Template::LogBase:
            return typeset::row({text(QStringLiteral("log"), font), subscript(strut(font), box(0, small)),
                                 text(QStringLiteral("("), font), box(1, font), text(QStringLiteral(")"), font)});
        case Template::Abs: return typeset::row({text(QStringLiteral("|"), font), box(0, font), text(QStringLiteral("|"), font)});
        }
        return {};
    }
};

}  // namespace

Box input(const Entry& entry, const QFont& font, QRectF* caret) {
    Box b = InputLayout{entry}.row(entry.root(), font, 0, true);
    for (int i = 0; i < b.runs.size(); ++i) {
        if (b.runs[i].role != Role::Caret) continue;
        const QFontMetricsF m(b.runs[i].font);
        if (caret) *caret = QRectF(b.runs[i].origin.x(), b.runs[i].origin.y() - m.ascent(), 0, m.ascent() + m.descent());
        b.runs.removeAt(i);
        break;
    }
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
