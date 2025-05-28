#include "databasemanager.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager()
    : m_database(QSqlDatabase::addDatabase("QSQLITE")) {}

void DatabaseManager::setDatabase(const QSqlDatabase &newDatabase)
{
    m_database = newDatabase;
}

DatabaseManager::~DatabaseManager() {
    if (m_database.isOpen()) {
        m_database.close();
    }
}

QSqlDatabase& DatabaseManager::database() {
    return m_database;
}

bool DatabaseManager::openDatabase(const QString& dbPath) {
    if (m_database.isOpen()) {
        // Enable foreign key constraints
        QSqlQuery query(m_database);
        if (!query.exec("PRAGMA foreign_keys = ON;")) {
            qCritical() << "Failed to enable foreign keys:" << query.lastError().text();
            return false;
        }
        qDebug() << "Foreign key support enabled in SQLite.";
        return true;
    }
    m_database.setDatabaseName(dbPath);
    if (!m_database.open()) {
        qCritical() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::closeDatabase() {
    if (m_database.isOpen()) {
        m_database.close();
    }
}
