#ifndef ICONS_H
#define ICONS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Monochrome SVG icons from :/icons, tinted to a colour at runtime
namespace Icons {

QPixmap tinted(const QString& name, const QColor& color, int size, qreal devicePixelRatio);
QIcon icon(const QString& name, const QColor& color, int size = 16);

// The Aion logo (its own colours, not tinted)
QPixmap logo(int size, qreal devicePixelRatio);
QIcon appIcon();                                     // Window and taskbar icon

} // namespace Icons

#endif // ICONS_H
