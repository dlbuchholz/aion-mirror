#ifndef CATEGORY_H
#define CATEGORY_H

#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

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
    QList<Category> getAllCategories();
    bool deleteCategory(int categoryId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // CATEGORY_H
