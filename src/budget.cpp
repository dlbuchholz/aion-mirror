#include "db/databasemanager.h"
#include "budget.h"

// Budget Implementation
Budget::Budget(int id, int userId, const QString& category, double amount, const QDate& startDate, const QDate& endDate)
    : budgetId(id), userId(userId), category(category), amount(amount), startDate(startDate), endDate(endDate) {}

int Budget::getBudgetId() const {
    return budgetId;
}

int Budget::getUserId() const { return userId; }

QString Budget::getCategory() const {
    return category;
}

double Budget::getAmount() const {
    return amount;
}

QDate Budget::getStartDate() const {
    return startDate;
}

QDate Budget::getEndDate() const {
    return endDate;
}

// BudgetDAO Implementation
BudgetDAO::BudgetDAO() {}

bool BudgetDAO::createBudget(const Budget& budget) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "INSERT INTO budgets (id, user_id, category, amount, start_date, end_date) "
        "VALUES (:id, :user_id, :category, :amount, :start_date, :end_date)"
        );
    query.bindValue(":id", budget.getBudgetId());
    query.bindValue(":user_id", budget.getUserId());
    query.bindValue(":category", budget.getCategory());
    query.bindValue(":amount", budget.getAmount());
    query.bindValue(":start_date", budget.getStartDate().toString("yyyy-MM-dd"));
    query.bindValue(":end_date", budget.getEndDate().toString("yyyy-MM-dd"));

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

QList<Budget> BudgetDAO::getAllBudgets(int userId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, user_id, category, amount, start_date, end_date FROM budgets WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);

    QList<Budget> budgets;
    if (query.exec()) {
        while (query.next()) {
            budgets.append(Budget(
                query.value("id").toInt(),
                query.value("user_id").toInt(),
                query.value("category").toString(),
                query.value("amount").toDouble(),
                QDate::fromString(query.value("start_date").toString(), "yyyy-MM-dd"),
                QDate::fromString(query.value("end_date").toString(), "yyyy-MM-dd")
                ));
        }
    } else {
        handleError(query);
    }
    return budgets;
}

bool BudgetDAO::deleteBudget(int budgetId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM budgets WHERE id = :id");
    query.bindValue(":id", budgetId);

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return query.numRowsAffected() == 1;
}

void BudgetDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
