#include "typeset.hpp"

#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QtNumeric>

namespace typeset {

namespace {

// Adds `part` to `box`, moved by (dx, dy).
void place(Box& box, const Box& part, qreal dx, qreal dy) {
    for (Run run : part.runs) {
        run.origin += QPointF(dx, dy);
        box.runs << run;
    }
    for (const QLineF& line : part.lines) box.lines << line.translated(dx, dy);
    for (Mark mark : part.marks) {
        mark.caret.translate(dx, dy);
        mark.item.translate(dx, dy);
        box.marks << mark;
    }
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

using Path = std::vector<std::pair<int, int>>;

// Lays out an entry's rows and templates, marking every place for the cursor.
struct InputLayout {
    bool decimalComma = false;

    QString shown(const QString& piece) const {
        if (!decimalComma) return piece;
        if (piece == ".") return QStringLiteral(",");
        if (piece == ", ") return QStringLiteral("; ");
        return piece;
    }

    // `path`: how to reach this row, as Entry::path().
    Box row(const Row& items, const QFont& font, const Path& path) const {
        QList<Box> parts;
        for (std::size_t i = 0; i < items.size(); ++i) parts << item(items[i], font, path, static_cast<int>(i));
        if (items.empty()) parts << (path.empty() ? strut(font) : text(QStringLiteral("□"), font));  // □: an empty template box
        Box b = typeset::row(parts);
        const QFontMetricsF m(font);
        qreal x = 0;
        for (int i = 0; i <= static_cast<int>(items.size()); ++i) {
            Mark mark{Position{path, i}, QRectF(x, -m.ascent(), 0, m.ascent() + m.descent()), QRectF()};
            if (i < static_cast<int>(items.size())) {
                const Box& part = parts[i];
                mark.item = QRectF(x, -part.ascent, part.width, part.ascent + part.descent);
                x += part.width;
            }
            b.marks << mark;
        }
        return b;
    }

    Box item(const Item& it, const QFont& font, const Path& path, int index) const {
        const QFont small = scaled(font, 0.7);
        const auto box = [&](int b, const QFont& f) {
            Path inner = path;
            inner.emplace_back(index, b);
            return row(it.boxes[static_cast<std::size_t>(b)], f, inner);
        };
        switch (it.kind) {
        case Template::Text: return text(shown(it.text), font);
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
        case Template::Sum:
        case Template::Product:
            return typeset::row({bigOperator(it.kind == Template::Sum ? QStringLiteral("Σ") : QStringLiteral("Π"),
                                             typeset::row({text(QStringLiteral("x="), small), box(0, small)}), box(1, small), font),
                                 text(QStringLiteral("("), font), box(2, font), text(QStringLiteral(")"), font)});
        }
        return {};
    }
};

}  // namespace

Box input(const Entry& entry, const QFont& font, QRectF* caret, bool decimalComma) {
    const Box b = InputLayout{decimalComma}.row(entry.root(), font, {});
    if (caret) {
        for (const Mark& mark : b.marks)
            if (mark.at == entry.position()) *caret = mark.caret;
    }
    return b;
}

Position hit(const Box& input, QPointF point) {
    Position nearest;
    qreal nearestDy = qInf();  // to the mark's line
    qreal nearestDx = qInf();  // along it
    for (const Mark& mark : input.marks) {
        const qreal dy = qMax<qreal>(0, qMax(mark.caret.top() - point.y(), point.y() - mark.caret.bottom()));
        const qreal dx = qAbs(point.x() - mark.caret.left());
        if (dy < nearestDy || (dy == nearestDy && dx < nearestDx)) {
            nearestDy = dy;
            nearestDx = dx;
            nearest = mark.at;
        }
    }
    return nearest;
}

QRectF selectionRect(const Box& input, const Entry& entry) {
    if (!entry.hasSelection()) return {};
    const int from = qMin(entry.anchor(), entry.cursor());
    const int to = qMax(entry.anchor(), entry.cursor());
    QRectF covered;
    for (const Mark& mark : input.marks)
        if (mark.at.path == entry.path() && mark.at.index >= from && mark.at.index < to) covered |= mark.item;
    return covered;
}

namespace {

// A tail after the value (its exponent, its ± U): on the same line when it fits, else at the start of the next.
void append(Box& b, const Box& tail, const QFont& font, qreal maxWidth) {
    QPointF at = end(b);
    if (maxWidth > 0 && at.x() + tail.width > maxWidth) at = QPointF(0, at.y() + QFontMetricsF(font).lineSpacing());
    place(b, tail, at.x(), at.y());
    b.width = qMax(b.width, at.x() + tail.width);
    b.ascent = qMax(b.ascent, tail.ascent - at.y());
    b.descent = qMax(b.descent, at.y() + tail.descent);
}

// " ± U", with U's ×10 power when it has one.
Box plusMinus(const QString& uncertainty, const QString& exponent, const QFont& font) {
    const Box u = text(QStringLiteral(" ± ") + uncertainty, font);
    if (exponent.isEmpty()) return u;
    return row({u, superscript(text(QStringLiteral("×10"), font), text(exponent, smaller(font)))});
}

}  // namespace

Box value(const view::ValueParts& parts, const QFont& font, qreal maxWidth) {
    QList<Segment> segments{{parts.trusted, Role::Plain}};
    if (!parts.noise.isEmpty()) segments << Segment{QStringLiteral("|") + parts.noise, Role::Noise};
    Box b = paragraph(segments, font, maxWidth);
    if (!parts.exponent.isEmpty())
        append(b, superscript(text(QStringLiteral("×10"), font), text(parts.exponent, smaller(font))), font, maxWidth);
    if (!parts.uncertainty.isEmpty()) append(b, plusMinus(parts.uncertainty, parts.uncertaintyExponent, font), font, maxWidth);
    return b;
}

namespace {

// The fraction, then " = " and its decimal with the recurring digits overlined; a whole number alone.
// The decimal moves to its own line when the whole does not fit.
Box exactValue(const view::FractionParts& parts, const QFont& font, qreal maxWidth) {
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

}  // namespace

// The exact value, then its ± U when it has uncertain inputs.
Box exact(const view::FractionParts& parts, const QFont& font, qreal maxWidth) {
    Box b = exactValue(parts, font, maxWidth);
    if (!parts.uncertainty.isEmpty()) append(b, plusMinus(parts.uncertainty, parts.uncertaintyExponent, font), font, maxWidth);
    return b;
}

}  // namespace typeset
