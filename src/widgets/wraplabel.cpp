#include "wraplabel.h"

#include <QFontMetricsF>
#include <QRegularExpression>

WrapLabel::WrapLabel(QWidget* parent)
    : WrapLabel(QString(), parent) {}

WrapLabel::WrapLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent) {
    setWordWrap(true);
}

// Measured as QLabel measures when it wraps (QFontMetrics::boundingRect), not by adding up the
// letters' widths: with a display scaling those come out a little narrower, and given just
// that, the label would wrap its last word
QSize WrapLabel::forTextWidth(const QString& text) const {
    const QMargins margins = contentsMargins();
    const QRect line = fontMetrics().boundingRect(QRect(0, 0, QWIDGETSIZE_MAX, QWIDGETSIZE_MAX), Qt::TextSingleLine, text);
    const int width = line.width() + margins.left() + margins.right() + 2 * margin();
    return {width, heightForWidth(width)};
}

QSize WrapLabel::sizeHint() const {
    return forTextWidth(text());
}

QSize WrapLabel::minimumSizeHint() const {
    static const QRegularExpression space(R"(\s+)");
    const QFontMetricsF metrics(font());          // Fractions, so the longer of two near words wins
    QString longest;
    for (const QString& word : text().split(space, Qt::SkipEmptyParts)) {
        if (metrics.horizontalAdvance(word) > metrics.horizontalAdvance(longest)) longest = word;
    }
    // As high as one line, like a QLabel with word wrap: layouts ask for the height at the
    // width they give (heightForWidth), and as high as the narrowest form it would leave
    // a gap below it whenever there's room
    return {forTextWidth(longest).width(), sizeHint().height()};
}
