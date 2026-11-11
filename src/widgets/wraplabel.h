#pragma once

#include <QLabel>

// A label that stays on one line while there's room and wraps onto more lines when there isn't.
// A QLabel with word wrap asks for less width than its text needs, so next to other widgets it
// wraps even with room to spare; this one asks for the whole line. It never asks for less than
// its longest word, so it doesn't break a word.
class WrapLabel : public QLabel {
    Q_OBJECT

public:
    explicit WrapLabel(QWidget* parent = nullptr);
    explicit WrapLabel(const QString& text, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QSize forTextWidth(const QString& text) const;
};
