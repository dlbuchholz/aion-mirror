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

    void setDatabase(const QSqlDatabase &newDatabase);

private:
    DatabaseManager();
    QSqlDatabase m_database;
};

#endif // DATABASEMANAGER_H
