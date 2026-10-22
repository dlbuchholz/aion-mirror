#include "databasesession.h"
#include "databasemanager.h"
#include "migrationmanager.h"
#include "settings.h"

#include <QDir>
#include <QFileInfo>
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
        if (errorMessage) *errorMessage = "The file couldn't be opened as a database.";
        return false;
    }
    if (isForeignDatabase()) {
        manager.closeDatabase();
        if (errorMessage) *errorMessage = "This file belongs to another program, not Aion.";
        return false;
    }
    if (!MigrationManager(BuiltInMigrations).applyMigrations()) {
        manager.closeDatabase();
        if (errorMessage) *errorMessage = "The database couldn't be updated to the current version. See the log for details.";
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
    return switchTo(target, errorMessage);
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
