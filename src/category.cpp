#include "db/databasemanager.h"
#include "Category.h"

// Category Implementation
Category::Category(int id, const QString& name, const QString& type)
    : categoryId(id), name(name), type(type) {}

int Category::getCategoryId() const {
    return categoryId;
}

QString Category::getName() const {
    return name;
}

QString Category::getType() const {
    return type;
}

// CategoryDAO Implementation
CategoryDAO::CategoryDAO() {}

bool CategoryDAO::createCategory(const Category& category) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO categories (id, name, type) VALUES (:id, :name, :type)");
    query.bindValue(":id", category.getCategoryId());
    query.bindValue(":name", category.getName());
    query.bindValue(":type", category.getType());

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

QList<Category> CategoryDAO::getAllCategories() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, name, type FROM categories");

    QList<Category> categories;
    if (query.exec()) {
        while (query.next()) {
            categories.append(Category(
                query.value("id").toInt(),
                query.value("name").toString(),
                query.value("type").toString()
                ));
        }
    } else {
        handleError(query);
    }
    return categories;
}

bool CategoryDAO::deleteCategory(int categoryId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM categories WHERE id = :id");
    query.bindValue(":id", categoryId);

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

void CategoryDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
