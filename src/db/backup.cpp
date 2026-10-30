#include "backup.h"
#include "databasemanager.h"
#include "migrationmanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

namespace {

constexpr int KeptAutomaticBackups = 5;              // Of each kind
const QString TimestampFormat = QStringLiteral("yyyy-MM-dd-HHmmss");

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

// Saves an automatic backup of this kind next to the open database and removes older ones of the kind
bool saveAutomatic(const QString& kind, QString* reason) {
    const QString databasePath = DatabaseManager::instance().database().databaseName();
    const QDir folder(Backup::folderFor(databasePath));
    const QString prefix = QString("%1-%2-").arg(QFileInfo(databasePath).completeBaseName(), kind);

    if (!QDir().mkpath(folder.path())) {
        setError(reason, "The backups folder couldn't be created.");
        return false;
    }
    // Never over another backup from the same second: it might be the one being restored
    QDateTime when = QDateTime::currentDateTime();
    while (folder.exists(prefix + when.toString(TimestampFormat) + ".db")) {
        when = when.addSecs(1);
    }
    if (!Backup::write(folder.filePath(prefix + when.toString(TimestampFormat) + ".db"), reason)) {
        return false;
    }

    // The dates in the names sort newest first
    const QStringList kept = folder.entryList({prefix + "*.db"}, QDir::Files, QDir::Name | QDir::Reversed);
    for (qsizetype i = KeptAutomaticBackups; i < kept.size(); ++i) {
        QFile::remove(folder.filePath(kept[i]));
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

QString folderFor(const QString& databasePath) {
    return QFileInfo(databasePath).absoluteDir().filePath("backups");
}

bool beforeUpdate(QString* errorMessage) {
    QString reason;
    if (!saveAutomatic("before-update", &reason)) {
        setError(errorMessage, QString("It needs an update, but couldn't be backed up first, so it wasn't changed. %1").arg(reason));
        return false;
    }
    return true;
}

bool beforeRestore(QString* errorMessage) {
    QString reason;
    if (!saveAutomatic("before-restore", &reason)) {
        setError(errorMessage, QString("Your current data couldn't be backed up first, so nothing was changed. %1").arg(reason));
        return false;
    }
    return true;
}

QList<AutomaticBackup> automaticBackups(const QString& databasePath) {
    const QRegularExpression pattern(
        QString("^%1-(before-update|before-restore)-(\\d{4}-\\d{2}-\\d{2}-\\d{6})\\.db$")
            .arg(QRegularExpression::escape(QFileInfo(databasePath).completeBaseName())));

    QList<AutomaticBackup> backups;
    const QFileInfoList files = QDir(folderFor(databasePath)).entryInfoList({"*.db"}, QDir::Files);
    for (const QFileInfo& file : files) {
        const QRegularExpressionMatch match = pattern.match(file.fileName());
        if (match.hasMatch()) {
            backups.append({file.absoluteFilePath(),
                            QDateTime::fromString(match.captured(2), TimestampFormat),
                            match.captured(1) == "before-update" ? "Before update" : "Before restore",
                            file.size()});
        }
    }
    std::sort(backups.begin(), backups.end(), [](const AutomaticBackup& a, const AutomaticBackup& b) {
        return a.when > b.when;
    });
    return backups;
}

std::optional<Contents> inspect(const QString& path, QString* errorMessage) {
    const QString connection = "backupInspection";
    std::optional<Contents> contents;
    QString problem;
    {
        QSqlDatabase file = QSqlDatabase::addDatabase("QSQLITE", connection);
        file.setDatabaseName(path);
        file.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!QFile::exists(path) || !file.open()) {
            problem = "The file couldn't be opened.";
        } else {
            QSqlQuery query(file);
            const auto count = [&query](const QString& table, int* result) {
                if (!query.exec(QString("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
                    return false;
                }
                *result = query.value(0).toInt();
                return true;
            };
            Contents found;
            if (!query.exec("PRAGMA integrity_check") || !query.next() || query.value(0).toString() != "ok") {
                problem = "The file isn't a database, or it's damaged.";
            } else if (!query.exec("SELECT MAX(version) FROM schema_migrations") || !query.next()) {
                problem = "This file isn't an Aion database.";
            } else if (query.value(0).toInt() > MigrationManager(BuiltInMigrations).newestKnownVersion()) {
                problem = "It's from a newer version of Aion. Update Aion to restore it.";
            } else if (!count("users", &found.users) || !count("accounts", &found.accounts)
                       || !count("transactions", &found.transactions)) {
                problem = "This file isn't an Aion database.";
            } else {
                contents = found;
            }
        }
        file.close();
    }
    QSqlDatabase::removeDatabase(connection);

    if (!contents) {
        setError(errorMessage, problem);
    }
    return contents;
}

} // namespace Backup
