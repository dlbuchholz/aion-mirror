#include "migrationmanager.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSqlRecord>

// Splits a script into statements at ';', leaving out "-- ..." comments. Semicolons and
// "--" inside quoted strings or comments don't count.
static QStringList splitSqlStatements(const QString& script) {
    QStringList statements;
    QString current;
    bool inString = false;
    for (qsizetype i = 0; i < script.size(); ++i) {
        const QChar c = script[i];
        if (c == '\'') {
            inString = !inString;                    // '' inside a string toggles twice, which is fine
        } else if (!inString && c == '-' && i + 1 < script.size() && script[i + 1] == '-') {
            while (i < script.size() && script[i] != '\n') ++i;
            current += '\n';
            continue;
        } else if (!inString && c == ';') {
            statements << current;
            current.clear();
            continue;
        }
        current += c;
    }
    statements << current;
    return statements;
}

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

bool MigrationManager::fail(const QString& reason) {
    qCritical().noquote() << reason;
    m_lastError = reason;
    return false;
}

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
        return fail("Failed to open migration file: " + migrationFile);
    }

    QTextStream in(&file);

    // Split the script into individual statements ...
    // For SQLite, the query string can contain only one statement at a time.
    // If more than one statements are give, the function returns false.
    QStringList scriptQueries = splitSqlStatements(in.readAll());
    file.close();

    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        return fail("Failed to start migration transaction: " + database.lastError().text());
    }
    QSqlQuery query(database);

    for (const QString& queryTxt : scriptQueries) {
        QString stmt = stripLeadingSqlComments(queryTxt);
        if (stmt.isEmpty()) {
            continue;
        }

        if (!query.exec(stmt)) {
            const QString reason = migrationFile + ": " + query.lastError().text();
            qCritical() << "Failed SQL:" << stmt;
            database.rollback();
            return fail(reason);
        }
        query.finish();// Ensure the query is finished before proceeding
    }

    // Insert the migration version into the schema_migrations table
    QSqlQuery insertQuery(database);
    insertQuery.prepare("INSERT INTO schema_migrations (version) VALUES (:version)");
    insertQuery.bindValue(":version", version);
    if (!insertQuery.exec()) {
        const QString reason = "Failed to record migration " + migrationFile + ": " + insertQuery.lastError().text();
        database.rollback();
        return fail(reason);
    }

    if (!database.commit()) {
        const QString reason = "Failed to commit migration " + migrationFile + ": " + database.lastError().text();
        database.rollback();
        return fail(reason);
    }

    qDebug() << "Applied migration:" << migrationFile;
    return true;
}


bool MigrationManager::needsUpdate() {
    const QVector<int> appliedMigrations = getAppliedMigrations();
    return !appliedMigrations.isEmpty() && !getPendingMigrations(appliedMigrations).isEmpty();
}

int MigrationManager::newestKnownVersion() const {
    int newestKnown = 0;
    for (const QString& fileName : QDir(m_migrationDir).entryList({"*.sql"}, QDir::Files)) {
        newestKnown = qMax(newestKnown, fileName.section('_', 0, 0).toInt());
    }
    return newestKnown;
}

bool MigrationManager::isNewerThanApp() {
    const int newestKnown = newestKnownVersion();
    for (int version : getAppliedMigrations()) {
        if (version > newestKnown) {
            return true;
        }
    }
    return false;
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

    return validateSchema();
}

QString MigrationManager::missingRequiredObject(const QSqlDatabase& database) {
    static const QStringList requiredObjects = {
        "users", "database_settings", "account_types", "accounts", "categories", "transactions",
        "budgets", "rules", "rule_conditions", "recurring_transactions", "schema_migrations", "account_balances"
    };
    QSqlQuery objectQuery(database);
    objectQuery.prepare("SELECT COUNT(*) FROM sqlite_master WHERE name = :name");
    for (const QString& name : requiredObjects) {
        objectQuery.bindValue(":name", name);
        if (!objectQuery.exec() || !objectQuery.next() || objectQuery.value(0).toInt() != 1) {
            return name;
        }
        objectQuery.finish();
    }
    return QString();
}

bool MigrationManager::validateSchema() {
    QSqlDatabase database = DatabaseManager::instance().database();
    const QString missing = missingRequiredObject(database);
    if (!missing.isEmpty()) {
        return fail("Required database object is missing: " + missing);
    }

    QSqlQuery foreignKeyCheck(database);
    if (!foreignKeyCheck.exec("PRAGMA foreign_key_check")) {
        return fail("Could not validate database foreign keys: " + foreignKeyCheck.lastError().text());
    }
    if (foreignKeyCheck.next()) {
        return fail("Database contains a foreign-key violation in table: " + foreignKeyCheck.value(0).toString());
    }
    return true;
}
