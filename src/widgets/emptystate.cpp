#include "emptystate.h"
#include "icons.h"
#include "theme.h"

#include <QPainter>

EmptyState::EmptyState(const QString& icon, const QString& text, QWidget* parent) : QLabel(text, parent), icon(icon) {
    setAlignment(Qt::AlignCenter);
    setWordWrap(true);
    setForegroundRole(QPalette::PlaceholderText);
    // Room for the icon above the text: the text centres in what's below it, so icon and text
    // together are centred
    setContentsMargins(16, IconSize + Gap, 16, 0);
    setMinimumHeight(180);
}

QRect EmptyState::iconRect() const {
    // The text is centred in what's below the margin for the icon; the icon goes just above it
    const QRect below = contentsRect();
    const int textHeight = fontMetrics().boundingRect(below, int(alignment()) | Qt::TextWordWrap, text()).height();
    const int textTop = below.top() + (below.height() - textHeight) / 2;
    return QRect((width() - IconSize) / 2, qMax(0, textTop - Gap - IconSize), IconSize, IconSize);
}

void EmptyState::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.drawPixmap(iconRect(), Icons::tinted(icon, Theme::colors().textMuted, IconSize, devicePixelRatioF()));
}
