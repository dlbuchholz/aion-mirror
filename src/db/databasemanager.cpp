#include "databasemanager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

// the app should call openDatabase(), tests can inject a database via setDatabase().
// Creating the default connection in the constructor causes duplicate default connection
// warnings when tests also call addDatabase().
DatabaseManager::DatabaseManager()
    : m_database() {}

void DatabaseManager::setDatabase(const QSqlDatabase &newDatabase)
{
    m_database = newDatabase;
    ++m_generation;
}

DatabaseManager::~DatabaseManager() {
    if (m_database.isOpen()) {
        m_database.close();
    }
}

QSqlDatabase& DatabaseManager::database() {
    return m_database;
}

bool DatabaseManager::commitOrRollback(QSqlDatabase& database) {
    if (database.commit()) {
        return true;
    }
    database.rollback();
    return false;
}

bool DatabaseManager::openDatabase(const QString& dbPath) {
    // If no driver/connection has been set up yet, create one now.
    if (!m_database.isValid()) {
        m_database = QSqlDatabase::addDatabase("QSQLITE");
    }

    if (!m_database.isOpen()) {
        ++m_generation;
        m_database.setDatabaseName(dbPath);
        if (!m_database.open()) {
            qCritical() << "Failed to open database:" << m_database.lastError().text();
            return false;
        }
    }

    QSqlQuery query(m_database);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qCritical() << "Failed to enable foreign keys:" << query.lastError().text();
        m_database.close();
        return false;
    }
    if (!query.exec("PRAGMA foreign_keys") || !query.next() || query.value(0).toInt() != 1) {
        qCritical() << "SQLite foreign-key enforcement could not be verified:"
                    << query.lastError().text();
        m_database.close();
        return false;
    }

    qDebug() << "Foreign key support enabled in SQLite.";
    return true;
}

void DatabaseManager::closeDatabase() {
    if (m_database.isOpen()) {
        m_database.close();
        ++m_generation;
    }
}
