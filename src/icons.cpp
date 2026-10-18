#include "icons.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace Icons {

QPixmap tinted(const QString& name, const QColor& color, int size, qreal devicePixelRatio) {
    static QHash<QString, QPixmap> cache;
    const QString key = QString("%1|%2|%3|%4").arg(name, color.name(QColor::HexArgb)).arg(size).arg(devicePixelRatio);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QPixmap pixmap(QSize(size, size) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QSvgRenderer renderer(QString(":/icons/%1.svg").arg(name));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRectF(0, 0, size, size), color);
    painter.end();

    cache.insert(key, pixmap);
    return pixmap;
}

QIcon icon(const QString& name, const QColor& color, int size) {
    return QIcon(tinted(name, color, size, qApp->devicePixelRatio()));
}

} // namespace Icons
