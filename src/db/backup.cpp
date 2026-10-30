#include "backup.h"
#include "databasemanager.h"

#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace {

void setError(QString* errorMessage, const QString& message) {
    if (errorMessage) *errorMessage = message;
}

// Puts the finished copy in place, keeping the old file until that worked
bool replaceFile(const QString& finished, const QString& path) {
    const QString previous = path + ".old";
    const bool hadFile = QFile::exists(path);
    if (hadFile) {
        QFile::remove(previous);
        if (!QFile::rename(path, previous)) {
            return false;
        }
    }
    if (!QFile::rename(finished, path)) {
        if (hadFile) {
            QFile::rename(previous, path);
        }
        return false;
    }
    if (hadFile) {
        QFile::remove(previous);
    }
    return true;
}

} // namespace

namespace Backup {

QString suggestedName(const QString& databasePath, const QDate& date) {
    return QString("%1-%2.db").arg(QFileInfo(databasePath).completeBaseName(), date.toString("yyyy-MM-dd"));
}

bool write(const QString& path, QString* errorMessage) {
    QSqlDatabase database = DatabaseManager::instance().database();
    const QFileInfo target(path);
    if (target.exists() && target.canonicalFilePath() == QFileInfo(database.databaseName()).canonicalFilePath()) {
        setError(errorMessage, "A backup can't replace the database it's a copy of. Choose another file.");
        return false;
    }

    // VACUUM INTO won't write over a file, and a copy that fails halfway mustn't replace an older backup
    const QString unfinished = path + ".tmp";
    QFile::remove(unfinished);
    QSqlQuery query(database);
    query.prepare("VACUUM INTO :path");
    query.bindValue(":path", unfinished);
    if (!query.exec()) {
        qWarning() << "Backup failed:" << query.lastError().text();
        QFile::remove(unfinished);
        setError(errorMessage, "The backup couldn't be written. Check that the folder exists and there's enough space.");
        return false;
    }
    query.finish();

    if (!replaceFile(unfinished, path)) {
        QFile::remove(unfinished);
        setError(errorMessage, "The backup couldn't be saved under that name. The file may be in use.");
        return false;
    }
    return true;
}

} // namespace Backup
