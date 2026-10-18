#ifndef MONEY_H
#define MONEY_H

#include <QString>
#include <QtGlobal>
#include <optional>

// Money amounts in whole cents. Sums and comparisons stay exact, unlike double.
using Cents = qint64;

namespace Money {

Cents fromDouble(double value);                      // Rounds to the nearest cent (for spin boxes)
double toDouble(Cents cents);                        // For spin boxes and ratios only

// Locale-independent "1234.50" / "-3.10". Used for hashes, so it must never change.
QString toPlainString(Cents cents);

// Parses "1234", "1234.5" or "1234.567" (digits and at most one '.', no sign).
// More than two decimals are rounded half away from zero. Empty or invalid text gives nullopt.
std::optional<Cents> parsePlain(const QString& text);

} // namespace Money

#endif // MONEY_H
