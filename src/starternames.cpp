#include "starternames.h"
#include "db/databasemanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

namespace {

// The texts of a table's starters are translated in this context (AccountDAO::addDefaultAccountTypes,
// CategoryDAO::addDefaultCategories)
bool follow(const QString& table, const char* context) {
    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery rows(database);
    if (!rows.exec("SELECT id, starter FROM " + table + " WHERE starter IS NOT NULL")) {
        qWarning() << "Failed to read the starter names of" << table << rows.lastError().text();
        return false;
    }
    QSqlQuery rename(database);
    // OR IGNORE: a name the user already gave another one of theirs stays theirs. Only when it
    // changes, so opening the database doesn't count as a change (scheduled backups).
    rename.prepare("UPDATE OR IGNORE " + table + " SET name = :name WHERE id = :id AND name <> :same");
    while (rows.next()) {
        const QString name = QCoreApplication::translate(context, rows.value(1).toString().toUtf8().constData());
        rename.bindValue(":name", name);
        rename.bindValue(":same", name);
        rename.bindValue(":id", rows.value(0));
        if (!rename.exec()) {
            qWarning() << "Failed to rename a starter of" << table << rename.lastError().text();
            return false;
        }
    }
    return true;
}

} // namespace

namespace StarterNames {

bool followLanguage() {
    const bool accountTypes = follow("account_types", "Account");
    const bool categories = follow("categories", "Category");
    return accountTypes && categories;
}

} // namespace StarterNames
