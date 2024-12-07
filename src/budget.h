#ifndef BUDGET_H
#define BUDGET_H

#include <QString>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Budget Object
class Budget {
public:
    Budget(int id, const QString& category, double amount, const QDate& startDate, const QDate& endDate);

    int getBudgetId() const;
    QString getCategory() const;
    double getAmount() const;
    QDate getStartDate() const;
    QDate getEndDate() const;

private:
    int budgetId;
    QString category;
    double amount;
    QDate startDate;
    QDate endDate;
};

// Budget DAO
class BudgetDAO {
public:
    BudgetDAO();

    bool createBudget(const Budget& budget);
    QList<Budget> getAllBudgets();
    bool deleteBudget(int budgetId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // BUDGET_H
