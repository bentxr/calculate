#include "icons.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace icons {

QIcon drawn(Kind kind, const QColor& ink, int side) {
    const qreal ratio = qApp->devicePixelRatio();
    QPixmap pixmap(qRound(side * ratio), qRound(side * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal s = side;
    QPen pen(ink, s / 10.0, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);
    switch (kind) {
    case Kind::Settings: {  // a gear: a ring, eight teeth and a hole
        const QPointF centre(s / 2, s / 2);
        painter.drawEllipse(centre, 0.22 * s, 0.22 * s);
        pen.setWidthF(s / 7.0);
        pen.setCapStyle(Qt::FlatCap);
        painter.setPen(pen);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = qDegreesToRadians(45.0 * i);
            const QPointF direction(qCos(angle), qSin(angle));
            painter.drawLine(centre + 0.22 * s * direction, centre + 0.36 * s * direction);
        }
        pen.setWidthF(s / 10.0);
        painter.setPen(pen);
        painter.drawEllipse(centre, 0.08 * s, 0.08 * s);
        break;
    }
    case Kind::Keyboard: {  // a keyboard: its outline, two rows of keys and the space bar
        painter.drawRoundedRect(QRectF(0.1 * s, 0.28 * s, 0.8 * s, 0.44 * s), 0.06 * s, 0.06 * s);
        for (int row = 0; row < 2; ++row)
            for (int key = 0; key < 4; ++key) {
                const qreal x = (0.24 + 0.17 * key) * s, y = (0.4 + 0.11 * row) * s;
                painter.drawLine(QPointF(x, y), QPointF(x + 0.04 * s, y));
            }
        painter.drawLine(QPointF(0.32 * s, 0.62 * s), QPointF(0.68 * s, 0.62 * s));
        break;
    }
    case Kind::Search: {  // a magnifier: the lens and its handle
        const QPointF centre(0.42 * s, 0.42 * s);
        const qreal r = 0.26 * s;
        painter.drawEllipse(centre, r, r);
        const QPointF rim = centre + QPointF(r, r) / qSqrt(2.0);
        painter.drawLine(rim, QPointF(0.86 * s, 0.86 * s));
        break;
    }
    case Kind::Home: {  // a house: the roof and the walls with a door
        painter.drawPolyline(QPolygonF({QPointF(0.12 * s, 0.5 * s), QPointF(0.5 * s, 0.16 * s), QPointF(0.88 * s, 0.5 * s)}));
        painter.drawPolyline(QPolygonF({QPointF(0.24 * s, 0.42 * s), QPointF(0.24 * s, 0.84 * s), QPointF(0.76 * s, 0.84 * s),
                                        QPointF(0.76 * s, 0.42 * s)}));
        painter.drawPolyline(QPolygonF({QPointF(0.42 * s, 0.84 * s), QPointF(0.42 * s, 0.62 * s), QPointF(0.58 * s, 0.62 * s),
                                        QPointF(0.58 * s, 0.84 * s)}));
        break;
    }
    case Kind::Back:
    case Kind::Forward: {  // an arrow: its shaft and head, pointing left or right
        const qreal d = kind == Kind::Back ? 1.0 : -1.0;
        const auto at = [&](qreal x, qreal y) { return QPointF((0.5 + d * (x - 0.5)) * s, y * s); };
        painter.drawLine(at(0.16, 0.5), at(0.84, 0.5));
        painter.drawPolyline(QPolygonF({at(0.44, 0.22), at(0.16, 0.5), at(0.44, 0.78)}));
        break;
    }
    case Kind::Pan: {  // four arrows from the centre: the cross of a move
        painter.drawLine(QPointF(0.5 * s, 0.1 * s), QPointF(0.5 * s, 0.9 * s));
        painter.drawLine(QPointF(0.1 * s, 0.5 * s), QPointF(0.9 * s, 0.5 * s));
        const qreal h = 0.14 * s;
        for (const QPointF& tip : {QPointF(0.5 * s, 0.1 * s), QPointF(0.5 * s, 0.9 * s), QPointF(0.1 * s, 0.5 * s), QPointF(0.9 * s, 0.5 * s)}) {
            const QPointF back = (QPointF(0.5 * s, 0.5 * s) - tip) / (0.4 * s) * h;  // towards the centre
            const QPointF side(back.y(), -back.x());
            painter.drawPolyline(QPolygonF({tip + back + side, tip, tip + back - side}));
        }
        break;
    }
    case Kind::Zoom: {  // a magnifier over a dashed rectangle: the zoom to a rectangle
        QPen dashed = pen;
        dashed.setWidthF(s / 16.0);
        dashed.setStyle(Qt::DashLine);
        painter.setPen(dashed);
        painter.drawRect(QRectF(0.1 * s, 0.1 * s, 0.5 * s, 0.4 * s));
        painter.setPen(pen);
        const QPointF centre(0.5 * s, 0.5 * s);
        const qreal r = 0.2 * s;
        painter.drawEllipse(centre, r, r);
        painter.drawLine(centre + QPointF(r, r) / qSqrt(2.0), QPointF(0.88 * s, 0.88 * s));
        break;
    }
    case Kind::Save: {  // a tray and an arrow down into it
        painter.drawLine(QPointF(0.5 * s, 0.12 * s), QPointF(0.5 * s, 0.64 * s));
        painter.drawPolyline(QPolygonF({QPointF(0.28 * s, 0.42 * s), QPointF(0.5 * s, 0.64 * s), QPointF(0.72 * s, 0.42 * s)}));
        painter.drawPolyline(QPolygonF({QPointF(0.14 * s, 0.62 * s), QPointF(0.14 * s, 0.86 * s), QPointF(0.86 * s, 0.86 * s),
                                        QPointF(0.86 * s, 0.62 * s)}));
        break;
    }
    case Kind::Open: {  // a folder: its tab on the left of the top
        painter.drawPolygon(QPolygonF({QPointF(0.1 * s, 0.22 * s), QPointF(0.4 * s, 0.22 * s), QPointF(0.48 * s, 0.32 * s),
                                       QPointF(0.9 * s, 0.32 * s), QPointF(0.9 * s, 0.82 * s), QPointF(0.1 * s, 0.82 * s)}));
        painter.drawLine(QPointF(0.1 * s, 0.42 * s), QPointF(0.9 * s, 0.42 * s));
        break;
    }
    }
    return QIcon(pixmap);
}

}  // namespace icons
