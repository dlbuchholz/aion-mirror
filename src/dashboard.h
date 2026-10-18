#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <QDate>
#include <QList>
#include <QString>
#include "money.h"

// Income and expenses of one month
struct MonthFlow {
    QDate month;                                     // First day of the month
    Cents incomeCents = 0;
    Cents expenseCents = 0;
    int incomeCount = 0;
    int expenseCount = 0;
    Cents netCents() const { return incomeCents - expenseCents; }
};

struct CategorySpending {
    int categoryId = 0;                              // 0 = no category
    QString categoryName;
    Cents spentCents = 0;
};

struct RecentTransaction {
    int transactionId = 0;
    QDate date;
    QString payee;
    QString categoryName;                            // Empty without a category
    Cents signedCents = 0;                           // Income positive, expenses negative
};

// What the dashboard shows for a user, all relative to "today"
struct DashboardData {
    Cents totalBalanceCents = 0;                     // All accounts of the user
    MonthFlow thisMonth;
    MonthFlow lastMonth;
    Cents lastMonthToDateExpenseCents = 0;          // Last month up to the same day, for a fair comparison
    Cents dailyAverageCents = 0;                     // This month's expenses per day so far
    int daysLeftInMonth = 0;                         // After today
    QList<MonthFlow> cashFlow;                       // Oldest first, ending with this month
    QList<CategorySpending> spendingByCategory;      // This month, largest first
    QList<RecentTransaction> recent;                 // Newest first
};

namespace Dashboard {

DashboardData load(int userId, const QDate& today, int months = 6, int recentCount = 5);

// "Good morning" (5–11), "Good afternoon" (12–17), "Good evening" (18–4)
QString greeting(const QTime& time);

} // namespace Dashboard

#endif // DASHBOARD_H
