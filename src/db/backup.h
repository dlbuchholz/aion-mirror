#ifndef BACKUP_H
#define BACKUP_H

#include <QDate>
#include <QString>

// Copies of the open database
namespace Backup {

// "budget-2026-10-30.db" for /some/folder/budget.db
QString suggestedName(const QString& databasePath, const QDate& date);

// Writes a consistent copy of the open database to path, replacing a file that's there. Without
// a finished copy nothing changes at path. Not the database's own file.
bool write(const QString& path, QString* errorMessage = nullptr);

} // namespace Backup

#endif // BACKUP_H
