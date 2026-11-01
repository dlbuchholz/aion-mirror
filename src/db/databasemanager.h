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

    // A commit that fails leaves the transaction open, and then nothing else can start one
    static bool commitOrRollback(QSqlDatabase& database);

    // Goes up whenever a database is opened or closed, so caches of database values know to reload
    quint64 generation() const { return m_generation; }

private:
    DatabaseManager();
    QSqlDatabase m_database;
    quint64 m_generation = 0;
};

#endif // DATABASEMANAGER_H
