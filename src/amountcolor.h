#ifndef AMOUNTCOLOR_H
#define AMOUNTCOLOR_H

#include "category.h"
#include "money.h"
#include "settings.h"
#include "theme.h"

// The colour of a transaction's amount in a list: money in green; money out red when chosen in
// Settings, Appearance, otherwise like the text. Transfers and balance adjustments going out stay
// like the text, since they aren't spending.
inline QColor amountColor(Cents signedCents, const QString& categoryName) {
    const Theme::Colors colors = Theme::colors();
    if (signedCents > 0) return colors.positive;
    if (signedCents < 0 && Settings::instance().isExpensesInRed() && !isUncountedCategory(categoryName)) return colors.danger;
    return colors.textPrimary;
}

#endif // AMOUNTCOLOR_H
