#include "migrationmanager.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSqlRecord>

static QString stripLeadingSqlComments(QString s) {
    QString out = s;
    while (true) {
        QString t = out.trimmed();
        if (t.startsWith("--")) {
            int nl = out.indexOf('\n');
            if (nl == -1) return QString(); // whole chunk is a comment
            out = out.mid(nl + 1);
            continue;
        }
        return t;
    }
}

MigrationManager::MigrationManager(const QString& migrationDir)
    : m_migrationDir(migrationDir) {}

MigrationManager::~MigrationManager() {}

QVector<int> MigrationManager::getAppliedMigrations() {
    QVector<int> appliedMigrations;
    QSqlQuery query(DatabaseManager::instance().database());

    query.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");

    if (query.exec("SELECT version FROM schema_migrations")) {
        while (query.next()) {
            appliedMigrations.append(query.value(0).toInt());
        }
    } else {
        qCritical() << "Failed to fetch applied migrations:" << query.lastError().text();
    }
    return appliedMigrations;
}

QVector<QString> MigrationManager::getPendingMigrations(const QVector<int>& appliedMigrations) {
    QVector<QString> pendingMigrations;
    QDir dir(m_migrationDir);

    if (!dir.exists()) {
        qCritical() << "Migration directory does not exist:" << m_migrationDir;
        return pendingMigrations;
    }

    QStringList migrationFiles = dir.entryList({"*.sql"}, QDir::Files, QDir::Name);
    for (const QString& fileName : migrationFiles) {
        int version = fileName.section('_', 0, 0).toInt();
        if (!appliedMigrations.contains(version)) {
            pendingMigrations.append(fileName);
        }
    }
    return pendingMigrations;
}

bool MigrationManager::executeMigration(const QString& migrationFile, int version) {
    QFile file(m_migrationDir + "/" + migrationFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Failed to open migration file:" << migrationFile;
        return false;
    }

    QTextStream in(&file);

    // Split the script into individual statements ...
    // For SQLite, the query string can contain only one statement at a time.
    // If more than one statements are give, the function returns false.
    QStringList scriptQueries = in.readAll().split(';');
    file.close();

    QSqlDatabase database = DatabaseManager::instance().database();
    QSqlQuery query(database);

    for (const QString& queryTxt : scriptQueries) {
        QString stmt = stripLeadingSqlComments(queryTxt);
        if (stmt.isEmpty()) {
            continue;
        }

        if (!query.exec(stmt)) {
            qCritical() << "Failed to execute query from migration file:" << migrationFile << query.lastError().text();
            qCritical() << "Failed SQL:" << stmt;
            return false;
        }
        query.finish();// Ensure the query is finished before proceeding
    }

    // Insert the migration version into the schema_migrations table
    QSqlQuery insertQuery(database);
    insertQuery.prepare("INSERT INTO schema_migrations (version) VALUES (:version)");
    insertQuery.bindValue(":version", version);
    if (!insertQuery.exec()) {
        qCritical() << "Failed to record migration:" << migrationFile << insertQuery.lastError().text();
        return false;
    }

    qDebug() << "Applied migration:" << migrationFile;
    return true;
}


bool MigrationManager::applyMigrations() {
    QVector<int> appliedMigrations = getAppliedMigrations();
    QVector<QString> pendingMigrations = getPendingMigrations(appliedMigrations);

    for (const QString& migrationFile : pendingMigrations) {
        int version = migrationFile.section('_', 0, 0).toInt();

        if (!executeMigration(migrationFile, version)) {
            return false; // Stop if any migration fails
        }
    }

    return true;
}
