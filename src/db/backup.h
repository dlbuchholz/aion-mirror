#ifndef BACKUP_H
#define BACKUP_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

// Copies of the open database
namespace Backup {

// "budget-2026-10-30.db" for /some/folder/budget.db
QString suggestedName(const QString& databasePath, const QDate& date);

// Writes a consistent copy of the open database to path, replacing a file that's there. Without
// a finished copy nothing changes at path. Not the database's own file.
bool write(const QString& path, QString* errorMessage = nullptr);

// The folder of automatic backups, next to the database
QString folderFor(const QString& databasePath);

// Saves a copy there before migrations change the open database. Keeps the last few of these.
bool beforeUpdate(QString* errorMessage = nullptr);

// The same before a restore replaces the open database
bool beforeRestore(QString* errorMessage = nullptr);

struct AutomaticBackup {
    QString path;
    QDateTime when;
    QString kind;                                    // "Before update", "Before restore"
    qint64 sizeBytes = 0;
};

// This database's automatic backups, newest first
QList<AutomaticBackup> automaticBackups(const QString& databasePath);

struct Contents {
    int users = 0;
    int accounts = 0;
    int transactions = 0;
};

// What a file holds, if it's a healthy Aion database this version can open. Doesn't change the file.
std::optional<Contents> inspect(const QString& path, QString* errorMessage = nullptr);

} // namespace Backup

#endif // BACKUP_H
