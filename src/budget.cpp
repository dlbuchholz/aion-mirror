#include "budget.h"
#include "category.h"
#include "db/databasemanager.h"

#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <optional>

namespace {

struct BudgetRow {
    int id = 0;
    int categoryId = 0;
    Cents amount = 0;
    QString startMonth;
    QString endMonth;                                // Empty = open-ended
    bool repeats = true;
};

bool exec(QSqlQuery& query) {
    if (!query.exec()) {
        qWarning() << "BudgetDAO:" << query.lastError().text() << query.lastQuery();
        return false;
    }
    return true;
}

QList<BudgetRow> rowsFor(int userId, int categoryId = 0) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(QString("SELECT id, category_id, amount, start_month, end_month, repeats FROM budgets "
                          "WHERE user_id = :user_id%1").arg(categoryId > 0 ? " AND category_id = :category_id" : ""));
    query.bindValue(":user_id", userId);
    if (categoryId > 0) query.bindValue(":category_id", categoryId);

    QList<BudgetRow> rows;
    if (!exec(query)) return rows;
    while (query.next()) {
        rows.append({query.value(0).toInt(), query.value(1).toInt(), query.value(2).toLongLong(),
                     query.value(3).toString(), query.value(4).toString(), query.value(5).toBool()});
    }
    return rows;
}

// The row that decides a category's limit in a month: its own single-month row, else the
// latest repeating row that has started and not ended
std::optional<BudgetRow> effectiveRow(const QList<BudgetRow>& rows, int categoryId, const QString& month) {
    std::optional<BudgetRow> repeating;
    for (const BudgetRow& row : rows) {
        if (row.categoryId != categoryId) continue;
        if (!row.repeats && row.startMonth == month) {
            return row;
        }
        if (row.repeats && row.startMonth <= month && (row.endMonth.isEmpty() || row.endMonth >= month)
            && (!repeating || row.startMonth > repeating->startMonth)) {
            repeating = row;
        }
    }
    return repeating;
}

QString previousMonth(const QString& month) {
    return BudgetDAO::monthKey(QDate::fromString(month + "-01", "yyyy-MM-dd").addMonths(-1));
}

// Expenses per category (0 = no category) of the user's accounts in a month, less refunds, without
// balance adjustments and transfers. Below zero when more came back than was spent.
QHash<int, Cents> spendingByCategory(int userId, const QDate& month) {
    const QDate first(month.year(), month.month(), 1);
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COALESCE(t.category_id, 0), SUM(" + SpendingCentsSql + ") FROM transactions t "
                  "JOIN accounts a ON a.id = t.account_id LEFT JOIN categories c ON c.id = t.category_id "
                  "WHERE a.user_id = :user_id AND " + IsSpendingSql + " AND t.date BETWEEN :first AND :last AND "
                  + CountedAsIncomeOrSpendingSql + " "
                  "GROUP BY COALESCE(t.category_id, 0)");
    query.bindValue(":user_id", userId);
    query.bindValue(":first", first.toString("yyyy-MM-dd"));
    query.bindValue(":last", first.addMonths(1).addDays(-1).toString("yyyy-MM-dd"));

    QHash<int, Cents> spent;
    if (exec(query)) {
        while (query.next()) spent.insert(query.value(0).toInt(), query.value(1).toLongLong());
    }
    return spent;
}

// Palette name per category
QHash<int, QString> categoryColors() {
    QSqlQuery query(DatabaseManager::instance().database());
    QHash<int, QString> colors;
    if (query.exec("SELECT id, color FROM categories")) {
        while (query.next()) colors.insert(query.value(0).toInt(), query.value(1).toString());
    }
    return colors;
}

QHash<int, QString> categoryNames() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, name FROM categories");
    QHash<int, QString> names;
    if (exec(query)) {
        while (query.next()) names.insert(query.value(0).toInt(), query.value(1).toString());
    }
    return names;
}

bool insertRow(int userId, int categoryId, Cents amount, const QString& startMonth, const QString& endMonth, bool repeats) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO budgets (user_id, category_id, amount, start_month, end_month, repeats) "
                  "VALUES (:user_id, :category_id, :amount, :start_month, :end_month, :repeats)");
    query.bindValue(":user_id", userId);
    query.bindValue(":category_id", categoryId);
    query.bindValue(":amount", amount);
    query.bindValue(":start_month", startMonth);
    query.bindValue(":end_month", endMonth.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(endMonth));
    query.bindValue(":repeats", repeats ? 1 : 0);
    return exec(query);
}

bool run(const QString& sql, const QVariantMap& values) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);
    for (auto it = values.cbegin(); it != values.cend(); ++it) query.bindValue(it.key(), it.value());
    return exec(query);
}

bool deleteSingleMonth(int userId, int categoryId, const QString& month) {
    return run("DELETE FROM budgets WHERE user_id = :u AND category_id = :c AND repeats = 0 AND start_month = :m",
               {{":u", userId}, {":c", categoryId}, {":m", month}});
}

// Runs the steps in one database transaction
template <typename Steps>
bool inTransaction(Steps steps) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) return false;
    if (!steps()) {
        db.rollback();
        return false;
    }
    return db.commit();
}

} // namespace

QString BudgetDAO::monthKey(const QDate& date) {
    return date.toString("yyyy-MM");
}

QList<MonthBudget> BudgetDAO::budgetsForMonth(int userId, const QDate& month) {
    const QString key = monthKey(month);
    const QList<BudgetRow> rows = rowsFor(userId);
    const QHash<int, Cents> spent = spendingByCategory(userId, month);
    const QHash<int, QString> names = categoryNames();
    const QHash<int, QString> colors = categoryColors();

    QSet<int> categories;
    for (const BudgetRow& row : rows) categories.insert(row.categoryId);

    QList<MonthBudget> budgets;
    for (int categoryId : categories) {
        const std::optional<BudgetRow> row = effectiveRow(rows, categoryId, key);
        if (!row || row->amount <= 0) {
            continue;                                // No budget, or switched off for this month
        }
        MonthBudget budget;
        budget.categoryId = categoryId;
        budget.categoryName = names.value(categoryId);
        budget.categoryColor = colors.value(categoryId);
        budget.limitCents = row->amount;
        budget.spentCents = spent.value(categoryId);
        budget.repeats = row->repeats;
        budget.validFrom = QDate::fromString(row->startMonth + "-01", "yyyy-MM-dd");
        budgets.append(budget);
    }
    std::sort(budgets.begin(), budgets.end(), [](const MonthBudget& a, const MonthBudget& b) {
        return a.categoryName.compare(b.categoryName, Qt::CaseInsensitive) < 0;
    });
    return budgets;
}

QList<UnbudgetedSpending> BudgetDAO::unbudgetedSpending(int userId, const QDate& month) {
    QSet<int> budgeted;
    for (const MonthBudget& budget : budgetsForMonth(userId, month)) budgeted.insert(budget.categoryId);
    const QHash<int, QString> names = categoryNames();

    QList<UnbudgetedSpending> result;
    const QHash<int, Cents> spent = spendingByCategory(userId, month);
    for (auto it = spent.cbegin(); it != spent.cend(); ++it) {
        if (!budgeted.contains(it.key()) && it.value() > 0) {
            result.append({it.key(), it.key() == 0 ? QString("No category") : names.value(it.key()), it.value()});
        }
    }
    std::sort(result.begin(), result.end(), [](const UnbudgetedSpending& a, const UnbudgetedSpending& b) {
        return a.spentCents > b.spentCents;          // Largest first
    });
    return result;
}

bool BudgetDAO::setMonthlyLimit(int userId, int categoryId, const QDate& fromMonth, Cents limitCents) {
    if (limitCents <= 0) return false;
    const QString month = monthKey(fromMonth);
    return inTransaction([&]() {
        // The regular value replaces a single-month value in the month it is set
        if (!deleteSingleMonth(userId, categoryId, month)) return false;

        const QList<BudgetRow> rows = rowsFor(userId, categoryId);
        for (const BudgetRow& row : rows) {
            if (row.repeats && row.startMonth == month) {
                return run("UPDATE budgets SET amount = :a WHERE id = :id", {{":a", limitCents}, {":id", row.id}});
            }
        }
        // A limit that is running ends the month before, so earlier months keep it
        QString endMonth;
        for (const BudgetRow& row : rows) {
            if (row.repeats && row.startMonth < month && (row.endMonth.isEmpty() || row.endMonth >= month)) {
                endMonth = row.endMonth;
                if (!run("UPDATE budgets SET end_month = :e WHERE id = :id", {{":e", previousMonth(month)}, {":id", row.id}})) {
                    return false;
                }
            }
        }
        return insertRow(userId, categoryId, limitCents, month, endMonth, true);
    });
}

bool BudgetDAO::setLimitForMonth(int userId, int categoryId, const QDate& month, Cents limitCents) {
    if (limitCents <= 0) return false;
    const QString key = monthKey(month);
    return inTransaction([&]() {
        return deleteSingleMonth(userId, categoryId, key)
               && insertRow(userId, categoryId, limitCents, key, key, false);
    });
}

bool BudgetDAO::removeLimit(int userId, int categoryId, const QDate& month, bool fromThisMonthOn) {
    const QString key = monthKey(month);
    return inTransaction([&]() {
        if (!fromThisMonthOn) {
            // Only this month: a regular limit gets a zero for this month, otherwise nothing is left
            if (!deleteSingleMonth(userId, categoryId, key)) return false;
            const std::optional<BudgetRow> regular = effectiveRow(rowsFor(userId, categoryId), categoryId, key);
            return !regular || insertRow(userId, categoryId, 0, key, key, false);
        }

        // From this month on: drop everything that starts now or later, end what is running
        const QVariantMap ids = {{":u", userId}, {":c", categoryId}, {":m", key}};
        return run("DELETE FROM budgets WHERE user_id = :u AND category_id = :c AND start_month >= :m", ids)
               && run("UPDATE budgets SET end_month = :e WHERE user_id = :u AND category_id = :c AND repeats = 1 "
                      "AND start_month < :m AND (end_month IS NULL OR end_month >= :m)",
                      {{":u", userId}, {":c", categoryId}, {":m", key}, {":e", previousMonth(key)}});
    });
}
