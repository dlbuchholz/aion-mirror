#ifndef CATEGORY_H
#define CATEGORY_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Balance adjustments count towards balances but aren't income or spending:
// the starting balances of accounts, and corrections to match the bank's balance
inline const QString InitialBalanceCategory = "Initial balance";
inline const QString BalanceCorrectionCategory = "Balance correction";
inline bool isBalanceAdjustmentCategory(const QString& name) {
    return name == InitialBalanceCategory || name == BalanceCorrectionCategory;
}
// SQL condition on transactions "t" that leaves out balance adjustments
inline const QString NotBalanceAdjustmentSql =
    "COALESCE(t.category_id, 0) NOT IN (SELECT id FROM categories WHERE name IN ('" + InitialBalanceCategory + "', '"
    + BalanceCorrectionCategory + "'))";

// Category Object
class Category {
public:
    Category(int id, const QString& name, const QString& type);

    int getCategoryId() const;
    QString getName() const;
    QString getType() const; // "income" or "expense"

private:
    int categoryId;
    QString name;
    QString type;
};

// Category DAO
class CategoryDAO {
public:
    CategoryDAO();

    bool createCategory(const Category& category);
    int addCategory(const Category &category);
    QList<Category> getAllCategories();
    Category getCategoryById(int categoryId);
    bool editCategory(int id, const QString &newName, const QString &newType);
    bool deleteCategory(int categoryId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // CATEGORY_H
