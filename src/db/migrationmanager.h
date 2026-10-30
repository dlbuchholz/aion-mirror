#ifndef MIGRATIONMANAGER_H
#define MIGRATIONMANAGER_H

#include <QString>
#include <QVector>

// The migrations built into Aion (db/migrations as Qt resources)
inline const QString BuiltInMigrations = QStringLiteral(":/migrations");

class MigrationManager {
public:
    explicit MigrationManager(const QString& migrationDir);
    ~MigrationManager();

    bool applyMigrations();
    bool needsUpdate();                              // A database in use that has migrations to apply (not a new one)
    bool isNewerThanApp();                           // Has migrations this version of Aion doesn't know
    int newestKnownVersion() const;

private:
    QString m_migrationDir; // Directory containing migration files

    QVector<int> getAppliedMigrations();
    QVector<QString> getPendingMigrations(const QVector<int>& appliedMigrations);
    bool executeMigration(const QString& migrationFile, int version);
    bool validateSchema();

};

#endif // MIGRATIONMANAGER_H
