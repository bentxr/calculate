#include "icons.hpp"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
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
    }
    return QIcon(pixmap);
}

}  // namespace icons
