#ifndef CATEGORY_H
#define CATEGORY_H

#include <QHash>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Balance adjustments count towards balances but aren't income or spending:
// the starting balances of accounts, and corrections to match the bank's balance.
// Only the app sets them; rules and imports leave them alone.
inline const QString InitialBalanceCategory = "Initial balance";
inline const QString BalanceCorrectionCategory = "Balance correction";
inline bool isBalanceAdjustmentCategory(const QString& name) {
    return name == InitialBalanceCategory || name == BalanceCorrectionCategory;
}
// Money moved between the user's own accounts: not income or spending either, but set like any
// category, by hand, rules and imports
inline const QString TransferCategory = "Transfer";
// Neither income nor spending: balance adjustments and transfers
inline bool isUncountedCategory(const QString& name) {
    return isBalanceAdjustmentCategory(name) || name == TransferCategory;
}
// SQL condition on transactions "t" that leaves out balance adjustments (transfers stay: they're real payments)
inline const QString NotBalanceAdjustmentSql =
    "COALESCE(t.category_id, 0) NOT IN (SELECT id FROM categories WHERE name IN ('" + InitialBalanceCategory + "', '"
    + BalanceCorrectionCategory + "'))";
// SQL condition on transactions "t" that leaves out what isn't income or spending, for totals
inline const QString CountedAsIncomeOrSpendingSql =
    "COALESCE(t.category_id, 0) NOT IN (SELECT id FROM categories WHERE name IN ('" + InitialBalanceCategory + "', '"
    + BalanceCorrectionCategory + "', '" + TransferCategory + "'))";

// Category Object
class Category {
public:
    Category(int id, const QString& name, const QString& type, const QString& color = QString());

    int getCategoryId() const;
    QString getName() const;
    QString getType() const; // "income" or "expense"
    QString getColor() const; // Palette name (see CategoryColors), empty = neutral
    bool isBuiltIn() const;  // Balance adjustments and Transfer, which other features rely on: can't be renamed or deleted
    bool isBalanceAdjustment() const;   // Only the app sets these

private:
    int categoryId;
    QString name;
    QString type;
    QString color;
};

// How much a category is used, for the settings list and confirmations
struct CategoryUsage {
    int transactions = 0;
    int budgets = 0;                                 // Budget rows
};

// Category DAO. Categories belong to a user; everything here works on that user's only.
class CategoryDAO {
public:
    explicit CategoryDAO(int userId);

    // The categories a new user starts with, the built-in ones included
    static bool addDefaultCategories(int userId);

    int addCategory(const Category &category);
    QList<Category> getAllCategories();
    Category getCategoryById(int categoryId);
    bool editCategory(int id, const QString &newName, const QString &newType);
    bool editCategory(int id, const QString &newName, const QString &newType, const QString &newColor);
    bool deleteCategory(int categoryId);             // Fails while transactions or budgets use it

    QHash<int, CategoryUsage> usage();
    // Moves transactions to intoId and deletes fromId. Budgets of fromId move along unless
    // intoId has budgets of its own, which then win.
    bool mergeCategory(int fromId, int intoId);
    // Deletes the category with its budgets; its transactions keep no category
    bool deleteCategoryKeepingTransactions(int categoryId);

private:
    void handleError(const QSqlQuery& query);
    bool owns(int categoryId);

    int userId;
};

#endif // CATEGORY_H
