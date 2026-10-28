#include "categorytag.h"
#include "categorycolors.h"

#include <QPainter>
#include <QPainterPath>

CategoryTag::CategoryTag(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void CategoryTag::setCategory(const QString& newName, const QString& newColor) {
    name = newName;
    color = newColor;
    setAccessibleName(name);
    setAccessibleDescription(CategoryColors::label(color));   // "Teal", or "No colour"
    updateGeometry();
    update();
}

void CategoryTag::setTheme(bool forDark) {
    dark = forDark;
    update();
}

QFont CategoryTag::tagFont() const {
    QFont small = font();
    small.setPointSizeF(font().pointSizeF() * 0.9);
    return small;
}

QSize CategoryTag::sizeHint() const {
    return QSize(QFontMetrics(tagFont()).horizontalAdvance(name) + 18, 22);
}

void CategoryTag::paintEvent(QPaintEvent*) {
    const CategoryColors::Swatch swatch = dark ? CategoryColors::swatch(color, *dark) : CategoryColors::swatch(color);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF pill = QRectF(rect()).adjusted(0, 1, 0, -1);
    QPainterPath path;
    path.addRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    painter.fillPath(path, swatch.background);
    painter.setFont(tagFont());
    painter.setPen(swatch.text);
    painter.drawText(pill, Qt::AlignCenter, name);
}
