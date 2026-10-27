#ifndef ICONS_H
#define ICONS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QAbstractButton;

// Monochrome SVG icons from :/icons, tinted to a colour at runtime
namespace Icons {

QPixmap tinted(const QString& name, const QColor& color, int size, qreal devicePixelRatio);
QIcon icon(const QString& name, const QColor& color, int size = 16);

// Gives the button the icon in its text colour, and keeps it so when the theme changes. Symbols
// like ✕ as text would make Windows search its fonts for them, which takes up to a second.
void setThemedIcon(QAbstractButton* button, const QString& name, int size = 14);

// The Aion logo (its own colours, not tinted)
QPixmap logo(int size, qreal devicePixelRatio);
QIcon appIcon();                                     // Window and taskbar icon

} // namespace Icons

#endif // ICONS_H
