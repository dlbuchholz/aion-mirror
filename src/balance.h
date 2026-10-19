#ifndef BALANCE_H
#define BALANCE_H

#include <QDate>
#include <QString>
#include "money.h"

// Account balances on a given day and corrections to match the bank's balance
namespace Balance {

// Sum of all transactions of the account up to and including the date
Cents onDate(int accountId, const QDate& date);

// Adds a "Balance correction" transaction so the balance on the date changes by differenceCents.
// Returns the new transaction's ID, -1 on failure; does nothing (and returns 0) for a zero difference.
int addCorrection(int accountId, const QDate& date, Cents differenceCents, const QString& note);

} // namespace Balance

#endif // BALANCE_H
