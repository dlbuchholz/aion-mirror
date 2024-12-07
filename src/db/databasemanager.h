#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>

class DatabaseManager {
public:
    static DatabaseManager& instance();
    ~DatabaseManager();

    QSqlDatabase& database();

    bool openDatabase(const QString& dbPath);
    void closeDatabase();

private:
    DatabaseManager();
    QSqlDatabase m_database;
};

#endif // DATABASEMANAGER_H
