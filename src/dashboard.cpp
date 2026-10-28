#include "dashboard.h"
#include "account.h"
#include "category.h"
#include "db/databasemanager.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace {

QSqlQuery prepared(const QString& sql) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    return query;
}

bool exec(QSqlQuery& query) {
    if (!query.exec()) {
        qWarning() << "Dashboard:" << query.lastError().text() << query.lastQuery();
        return false;
    }
    return true;
}

QDate monthStart(const QDate& date) {
    return QDate(date.year(), date.month(), 1);
}

} // namespace

namespace Dashboard {

QString greeting(const QTime& time) {
    const int hour = time.hour();
    if (hour >= 5 && hour < 12) return "Good morning";
    if (hour >= 12 && hour < 18) return "Good afternoon";
    return "Good evening";
}

DashboardData load(int userId, const QDate& today, int months, int recentCount) {
    DashboardData data;

    AccountDAO accountDao;
    for (const Account& account : accountDao.getAllAccountsForUser(userId)) {
        data.totalBalanceCents += account.getBalanceCents();
    }

    // Income and expenses per month (without balance adjustments and transfers), from the first shown month to the end of this one
    const QDate first = monthStart(today).addMonths(-(qMax(months, 2) - 1));
    const QDate last = monthStart(today).addMonths(1).addDays(-1);
    QHash<QString, MonthFlow> flows;
    for (QDate month = first; month <= last; month = month.addMonths(1)) {
        MonthFlow flow;
        flow.month = month;
        flows.insert(month.toString("yyyy-MM"), flow);
    }
    QSqlQuery flowQuery = prepared(
        "SELECT substr(t.date, 1, 7), t.type, SUM(t.amount), COUNT(*) FROM transactions t "
        "JOIN accounts a ON a.id = t.account_id "
        "WHERE a.user_id = :user_id AND t.date BETWEEN :first AND :last AND " + CountedAsIncomeOrSpendingSql + " "
        "GROUP BY substr(t.date, 1, 7), t.type");
    flowQuery.bindValue(":user_id", userId);
    flowQuery.bindValue(":first", first.toString("yyyy-MM-dd"));
    flowQuery.bindValue(":last", last.toString("yyyy-MM-dd"));
    if (exec(flowQuery)) {
        while (flowQuery.next()) {
            auto it = flows.find(flowQuery.value(0).toString());
            if (it == flows.end()) continue;
            if (flowQuery.value(1).toString() == "income") {
                it->incomeCents = flowQuery.value(2).toLongLong();
                it->incomeCount = flowQuery.value(3).toInt();
            } else {
                it->expenseCents = flowQuery.value(2).toLongLong();
                it->expenseCount = flowQuery.value(3).toInt();
            }
        }
    }
    for (QDate month = first; month <= last; month = month.addMonths(1)) {
        data.cashFlow.append(flows.value(month.toString("yyyy-MM")));
    }
    data.thisMonth = data.cashFlow.last();
    data.lastMonth = data.cashFlow[data.cashFlow.size() - 2];
    data.cashFlow = data.cashFlow.mid(data.cashFlow.size() - qMax(months, 1));   // Last month was loaded for the comparison

    const QDate lastMonthStart = monthStart(today).addMonths(-1);
    QSqlQuery toDateQuery = prepared(
        "SELECT COALESCE(SUM(t.amount), 0) FROM transactions t JOIN accounts a ON a.id = t.account_id "
        "WHERE a.user_id = :user_id AND t.type = 'expense' AND t.date BETWEEN :first AND :last AND " + CountedAsIncomeOrSpendingSql);
    toDateQuery.bindValue(":user_id", userId);
    toDateQuery.bindValue(":first", lastMonthStart.toString("yyyy-MM-dd"));
    toDateQuery.bindValue(":last", lastMonthStart.addDays(qMin(today.day(), lastMonthStart.daysInMonth()) - 1).toString("yyyy-MM-dd"));
    if (exec(toDateQuery) && toDateQuery.next()) {
        data.lastMonthToDateExpenseCents = toDateQuery.value(0).toLongLong();
    }

    data.dailyAverageCents = data.thisMonth.expenseCents / today.day();
    data.daysLeftInMonth = today.daysInMonth() - today.day();

    // This month's expenses per category
    QSqlQuery categoryQuery = prepared(
        "SELECT COALESCE(t.category_id, 0), c.name, SUM(t.amount), c.color FROM transactions t "
        "JOIN accounts a ON a.id = t.account_id LEFT JOIN categories c ON c.id = t.category_id "
        "WHERE a.user_id = :user_id AND t.type = 'expense' AND t.date BETWEEN :first AND :last AND " + CountedAsIncomeOrSpendingSql + " "
        "GROUP BY COALESCE(t.category_id, 0)");
    categoryQuery.bindValue(":user_id", userId);
    categoryQuery.bindValue(":first", monthStart(today).toString("yyyy-MM-dd"));
    categoryQuery.bindValue(":last", last.toString("yyyy-MM-dd"));
    if (exec(categoryQuery)) {
        while (categoryQuery.next()) {
            const int categoryId = categoryQuery.value(0).toInt();
            data.spendingByCategory.append({categoryId,
                                            categoryId == 0 ? QString("No category") : categoryQuery.value(1).toString(),
                                            categoryQuery.value(2).toLongLong(), categoryQuery.value(3).toString()});
        }
    }
    std::sort(data.spendingByCategory.begin(), data.spendingByCategory.end(),
              [](const CategorySpending& a, const CategorySpending& b) { return a.spentCents > b.spentCents; });

    // Latest transactions; future-dated ones (planned payments) and balance adjustments don't count as recent
    QSqlQuery recentQuery = prepared(
        "SELECT t.id, t.date, t.payee, c.name, t.amount, t.type, c.color FROM transactions t "
        "JOIN accounts a ON a.id = t.account_id LEFT JOIN categories c ON c.id = t.category_id "
        "WHERE a.user_id = :user_id AND t.date <= :today AND " + NotBalanceAdjustmentSql + " "
        "ORDER BY t.date DESC, t.id DESC LIMIT :limit");
    recentQuery.bindValue(":user_id", userId);
    recentQuery.bindValue(":today", today.toString("yyyy-MM-dd"));
    recentQuery.bindValue(":limit", recentCount);
    if (exec(recentQuery)) {
        while (recentQuery.next()) {
            const Cents amount = recentQuery.value(4).toLongLong();
            data.recent.append({recentQuery.value(0).toInt(),
                                QDate::fromString(recentQuery.value(1).toString(), "yyyy-MM-dd"),
                                recentQuery.value(2).toString(), recentQuery.value(3).toString(),
                                recentQuery.value(5).toString() == "income" ? amount : -amount, recentQuery.value(6).toString()});
        }
    }
    return data;
}

} // namespace Dashboard
