#ifndef MIGRATIONMANAGER_H
#define MIGRATIONMANAGER_H

#include <QString>
#include <QVector>

class MigrationManager {
public:
    explicit MigrationManager(const QString& migrationDir);
    ~MigrationManager();

    bool applyMigrations();

private:
    QString m_migrationDir; // Directory containing migration files

    QVector<int> getAppliedMigrations();
    QVector<QString> getPendingMigrations(const QVector<int>& appliedMigrations);
    bool executeMigration(const QString& migrationFile, int version);
    bool validateSchema();

};

#endif // MIGRATIONMANAGER_H
