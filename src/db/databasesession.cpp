#include "databasesession.h"
#include "backup.h"
#include "databasemanager.h"
#include "migrationmanager.h"
#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

QString currentPath;

QString normalized(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

// A SQLite file that already has tables but no migration bookkeeping belongs to another program
bool isForeignDatabase() {
    QSqlQuery query(DatabaseManager::instance().database());
    if (!query.exec("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")
        || !query.next() || query.value(0).toInt() == 0) {
        return false;
    }
    return !query.exec("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'schema_migrations'")
           || !query.next();
}

bool openAndMigrate(const QString& path, QString* errorMessage) {
    DatabaseManager& manager = DatabaseManager::instance();
    manager.closeDatabase();

    if (!manager.openDatabase(path)) {
        if (errorMessage) *errorMessage = "The file couldn't be opened as a database. " + manager.database().lastError().text();
        return false;
    }
    if (isForeignDatabase()) {
        manager.closeDatabase();
        if (errorMessage) *errorMessage = "This file belongs to another program, not Aion.";
        return false;
    }
    MigrationManager migrations(BuiltInMigrations);
    if (migrations.isNewerThanApp()) {
        manager.closeDatabase();
        if (errorMessage) *errorMessage = "It was last used with a newer version of Aion. Update Aion to open it.";
        return false;
    }
    if (migrations.needsUpdate() && !Backup::beforeUpdate(errorMessage)) {
        manager.closeDatabase();
        return false;
    }
    if (!migrations.applyMigrations()) {
        manager.closeDatabase();
        if (errorMessage) *errorMessage = "The database couldn't be updated to the current version. " + migrations.lastError();
        return false;
    }
    return true;
}

bool switchTo(const QString& path, QString* errorMessage) {
    const QString previous = currentPath;
    if (!openAndMigrate(path, errorMessage)) {
        // Fall back to the database that was open before
        if (!previous.isEmpty() && previous != path) {
            openAndMigrate(previous, nullptr);
        }
        return false;
    }

    currentPath = path;
    Settings::instance().setLastDatabase(path);
    Settings::instance().addRecentDatabase(path);
    return true;
}

} // namespace

namespace DatabaseSession {

QString defaultDatabasePath() {
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(basePath);
    return normalized(basePath + "/app_data.db");
}

QString currentDatabasePath() {
    return currentPath;
}

bool open(const QString& path, QString* errorMessage) {
    const QString target = normalized(path);
    if (!QFileInfo::exists(target) && target != defaultDatabasePath()) {
        if (errorMessage) *errorMessage = "The file doesn't exist anymore.";
        return false;
    }
    if (!switchTo(target, errorMessage)) {
        return false;
    }
    // Not being able to back up on schedule is no reason to keep the database closed
    QString backupError;
    if (!Backup::scheduled(&backupError) && !backupError.isEmpty()) {
        qWarning() << "Scheduled backup failed:" << backupError;
    }
    return true;
}

bool openAtStartup(QString* warningMessage, QString* errorMessage) {
    const QString last = Settings::instance().getLastDatabase();
    QString lastError;
    if (!last.isEmpty()) {
        QString error;
        if (open(last, &error)) {
            return true;
        }
        qWarning() << "Could not open last database" << last << ":" << error;
        lastError = QString("Couldn't open %1. %2\n\nAion opened its default database instead.")
                        .arg(QFileInfo(last).fileName(), error);
    }

    QString error;
    if (!open(defaultDatabasePath(), &error)) {
        if (errorMessage) *errorMessage = QString("Couldn't open %1. %2").arg(QDir::toNativeSeparators(defaultDatabasePath()), error);
        return false;
    }
    if (warningMessage) *warningMessage = lastError;
    return true;
}

bool restore(const QString& backupPath, QString* errorMessage) {
    const QString target = currentPath;
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };

    if (QFileInfo(backupPath).canonicalFilePath() == QFileInfo(target).canonicalFilePath()) {
        return fail("That's the database you have open. Choose a backup instead.");
    }
    if (!Backup::inspect(backupPath, errorMessage)) {
        return false;
    }

    // Copied first: a failed copy changes nothing, and saving the current data below may remove
    // the oldest automatic backup, which may be this one
    const QString incoming = target + ".restoring";
    QFile::remove(incoming);
    if (!QFile::copy(backupPath, incoming)) {
        return fail("The backup couldn't be copied. Nothing was changed.");
    }
    if (!Backup::beforeRestore(errorMessage)) {
        QFile::remove(incoming);
        return false;
    }

    DatabaseManager& manager = DatabaseManager::instance();
    manager.closeDatabase();

    const QString replaced = target + ".replaced";
    QFile::remove(replaced);
    if (!QFile::rename(target, replaced)) {
        QFile::remove(incoming);
        openAndMigrate(target, nullptr);
        return fail("The database file couldn't be replaced. It may be in use. Nothing was changed.");
    }
    if (!QFile::rename(incoming, target) || !openAndMigrate(target, nullptr)) {
        manager.closeDatabase();
        QFile::remove(incoming);
        QFile::remove(target);
        QFile::rename(replaced, target);
        openAndMigrate(target, nullptr);
        return fail("The backup couldn't be opened, so your current data was put back.");
    }

    QFile::remove(replaced);
    return true;
}

bool create(const QString& path, QString* errorMessage) {
    const QString target = normalized(path);
    if (QFileInfo::exists(target)) {
        if (errorMessage) *errorMessage = "That file already exists. Use \"Open database file\" to open it.";
        return false;
    }
    return switchTo(target, errorMessage);
}

} // namespace DatabaseSession
