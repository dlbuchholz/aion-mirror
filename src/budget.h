#ifndef BUDGET_H
#define BUDGET_H

#include <QDate>
#include <QList>
#include <QString>
#include "money.h"

// One category's budget as it applies to a particular month
struct MonthBudget {
    int categoryId = 0;
    QString categoryName;
    Cents limitCents = 0;
    Cents spentCents = 0;
    bool repeats = true;                             // true: the regular monthly limit; false: set for this month only
    QDate validFrom;                                 // Repeating limits: first month this value applies to
    QString categoryColor;                           // Palette name, empty = neutral
    int transactionCount = 0;                        // The transactions in spentCents, refunds included
    Cents leftCents() const { return limitCents - spentCents; }
};

// Expenses in a month for a category without a budget (categoryId 0 = no category)
struct UnbudgetedSpending {
    int categoryId = 0;
    QString categoryName;
    Cents spentCents = 0;
    int transactionCount = 0;                        // The transactions in spentCents, refunds included
};

// One transaction counted in a category's spending
struct BudgetTransaction {
    int transactionId = 0;
    QDate date;
    QString payee;
    Cents spentCents = 0;                            // Below zero for money that came back
};

// How many of a month's days have passed by today, today included: 0 before it, all after it
inline int daysPassed(const QDate& month, const QDate& today) {
    const QDate first(month.year(), month.month(), 1);
    if (today < first) return 0;
    if (today >= first.addMonths(1)) return first.daysInMonth();
    return today.day();
}

// Monthly category limits. A regular limit applies from a month onward until it is changed;
// a single month can have its own value on top of that. Months are passed as any date in them.
class BudgetDAO {
public:
    QList<MonthBudget> budgetsForMonth(int userId, const QDate& month);          // Sorted by category name
    QList<UnbudgetedSpending> unbudgetedSpending(int userId, const QDate& month);
    // The transactions in a category's spending in a month (0 = no category), oldest first
    QList<BudgetTransaction> transactions(int userId, int categoryId, const QDate& month);

    // Every month from fromMonth on (earlier months keep their previous limit)
    bool setMonthlyLimit(int userId, int categoryId, const QDate& fromMonth, Cents limitCents);
    // Only this month; the regular limit applies again afterwards
    bool setLimitForMonth(int userId, int categoryId, const QDate& month, Cents limitCents);
    // No budget from this month on, or only in this month
    bool removeLimit(int userId, int categoryId, const QDate& month, bool fromThisMonthOn);

    static QString monthKey(const QDate& date);     // "2026-10"
};

#endif // BUDGET_H
