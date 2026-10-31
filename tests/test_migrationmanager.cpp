#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/migrationmanager.h"

namespace {

QStringList builtInMigrationFiles() {
    return QDir(BuiltInMigrations).entryList({"*.sql"}, QDir::Files, QDir::Name);
}

QStringList column(const QString& sql, int index = 0) {
    QStringList values;
    QSqlQuery query(DatabaseManager::instance().database());
    if (query.exec(sql)) {
        while (query.next()) values.append(query.value(index).toString());
    }
    return values;
}

// The open database's schema as sorted lines, so two databases compare the same when they have the
// same tables, columns, keys, indexes and views. Column order is left out: ALTER TABLE adds columns
// at the end, a new database has them where 001 lists them.
QStringList describeSchema() {
    QStringList lines;
    QSqlQuery query(DatabaseManager::instance().database());
    for (const QString& table : column("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")) {
        if (query.exec(QString("PRAGMA table_info(\"%1\")").arg(table))) {
            while (query.next()) {
                lines << QString("%1 column %2 %3 notnull=%4 default=%5 pk=%6")
                             .arg(table, query.value("name").toString(), query.value("type").toString().toUpper(),
                                  query.value("notnull").toString(), query.value("dflt_value").toString(),
                                  query.value("pk").toString());
            }
        }
        if (query.exec(QString("PRAGMA foreign_key_list(\"%1\")").arg(table))) {
            while (query.next()) {
                lines << QString("%1 foreign key %2 -> %3.%4 on delete %5")
                             .arg(table, query.value("from").toString(), query.value("table").toString(),
                                  query.value("to").toString(), query.value("on_delete").toString());
            }
        }
        if (query.exec(QString("PRAGMA index_list(\"%1\")").arg(table))) {
            QStringList indexes;
            while (query.next()) {
                indexes << QString("%1|%2").arg(query.value("name").toString(), query.value("unique").toString());
            }
            for (const QString& index : std::as_const(indexes)) {
                const QString name = index.section('|', 0, 0);
                // Automatic indexes of UNIQUE constraints are numbered in table order; name them by their columns
                const QString columns = column(QString("PRAGMA index_info(\"%1\")").arg(name), 2).join(",");
                lines << QString("%1 index %2 (%3) unique=%4")
                             .arg(table, name.startsWith("sqlite_autoindex") ? QString("auto") : name, columns,
                                  index.section('|', 1, 1));
            }
        }
    }
    for (const QString& view : column("SELECT sql FROM sqlite_master WHERE type IN ('view', 'trigger')")) {
        lines << view.simplified();
    }
    lines.sort();
    return lines;
}

using Snapshot = QMap<QPair<QString, QStringList>, QStringList>;   // (table, columns) -> rows

QStringList rowsOf(const QString& table, const QStringList& columns) {
    QSqlQuery query(DatabaseManager::instance().database());
    if (!query.exec(QString("SELECT \"%1\" FROM \"%2\" ORDER BY rowid").arg(columns.join("\", \""), table))) {
        return {"missing: " + query.lastError().text()};
    }
    QStringList rows;
    while (query.next()) {
        QStringList values;
        for (int i = 0; i < columns.size(); ++i) values << query.value(i).toString();
        rows << values.join(" | ");
    }
    return rows;
}

// Every row of every table, over the columns the table has now
Snapshot snapshotData() {
    Snapshot data;
    for (const QString& table : column("SELECT name FROM sqlite_master WHERE type = 'table' "
                                       "AND name NOT LIKE 'sqlite_%' AND name != 'schema_migrations'")) {
        const QStringList columns = column(QString("PRAGMA table_info(\"%1\")").arg(table), 1);
        data.insert({table, columns}, rowsOf(table, columns));
    }
    return data;
}

} // namespace

class TestMigrationManager : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { DatabaseManager::instance().closeDatabase(); }

    // A database at any earlier version, with data, upgrades to the same schema as a new one and
    // keeps all its data. The seed uses the columns of 001; when a migration changes those, adapt
    // the seed (data the user already has can't be adapted, so think twice).
    void upgradingFromEveryVersionKeepsTheData_data() {
        QTest::addColumn<int>("startFiles");
        const QStringList files = builtInMigrationFiles();
        for (int i = 1; i <= files.size(); ++i) {
            QTest::addRow("from %s", qPrintable(files[i - 1])) << i;   // The last row: already current
        }
    }

    void upgradingFromEveryVersionKeepsTheData() {
        QFETCH(int, startFiles);
        const QStringList files = builtInMigrationFiles();

        // What a new database looks like
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(DatabaseManager::instance().openDatabase(root.filePath("new.sqlite")));
        QVERIFY(MigrationManager(BuiltInMigrations).applyMigrations());
        const QStringList newSchema = describeSchema();
        DatabaseManager::instance().closeDatabase();

        // A database at an older version, in use
        QDir older(root.filePath("older"));
        QVERIFY(older.mkpath("."));
        for (int i = 0; i < startFiles; ++i) {
            QVERIFY(QFile::copy(BuiltInMigrations + "/" + files[i], older.filePath(files[i])));
        }
        QVERIFY(DatabaseManager::instance().openDatabase(root.filePath("upgraded.sqlite")));
        MigrationManager(older.path()).applyMigrations();   // Older versions may not pass today's checks
        QCOMPARE(column("SELECT COUNT(*) FROM schema_migrations").value(0).toInt(), startFiles);
        QSqlQuery seed(DatabaseManager::instance().database());
        for (const char* sql : {
                 "INSERT INTO users (id, username, name) VALUES (1, 'anna', 'Anna')",
                 "INSERT INTO database_settings (key, value) VALUES ('currency/symbol', 'EUR')",
                 "INSERT INTO account_types (id, user_id, name) VALUES (1, 1, 'Checking')",
                 "INSERT INTO accounts (id, user_id, name, type_id) VALUES (1, 1, 'Giro', 1)",
                 "INSERT INTO categories (id, user_id, name, type, color) VALUES (1, 1, 'Groceries', 'expense', 'teal')",
                 "INSERT INTO rules (id, user_id, position, match_all, category_id, new_payee) VALUES (1, 1, 0, 1, 1, 'Rewe')",
                 "INSERT INTO rule_conditions (rule_id, field, operator, value) VALUES (1, 'payee', 'contains', 'REWE')",
                 "INSERT INTO transactions (id, account_id, payee, original_payee, amount, date, description, category_id, type, hash) "
                 "VALUES (1, 1, 'Rewe', 'REWE MARKT 4711', 3240, '2026-10-01', 'Wocheneinkauf', 1, 'expense', 'abc')",
                 "INSERT INTO transactions (id, account_id, payee, amount, date, type) VALUES (2, 1, 'Gehalt', 245000, '2026-10-01', 'income')",
                 "INSERT INTO budgets (user_id, category_id, amount, start_month, end_month, repeats) "
                 "VALUES (1, 1, 30000, '2026-01', NULL, 1)",
                 "INSERT INTO recurring_transactions (transaction_id, frequency) VALUES (2, 'monthly')"}) {
            QVERIFY2(seed.exec(sql), qPrintable(seed.lastError().text() + "\n" + sql));
        }
        const Snapshot dataBefore = snapshotData();

        // Upgraded with all of today's migrations
        QVERIFY(MigrationManager(BuiltInMigrations).applyMigrations());
        QCOMPARE(column("SELECT version FROM schema_migrations ORDER BY version").size(), files.size());
        QCOMPARE(describeSchema(), newSchema);
        // The same tables and columns as before, as they are now
        for (auto it = dataBefore.cbegin(); it != dataBefore.cend(); ++it) {
            const QStringList now = rowsOf(it.key().first, it.key().second);
            QVERIFY2(now == it.value(), qPrintable(QString("%1 changed\n   before: %2\n   after:  %3")
                                                       .arg(it.key().first, it.value().join("; "), now.join("; "))));
        }
        QVERIFY(column("PRAGMA foreign_key_check").isEmpty());
    }

    void failedMigrationRollsBackStatements() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile migration(root.filePath("002_broken.sql"));
        QVERIFY(migration.open(QIODevice::WriteOnly | QIODevice::Text));
        migration.write("CREATE TABLE must_rollback (id INTEGER); INVALID SQL;");
        migration.close();

        QVERIFY(DatabaseManager::instance().openDatabase(root.filePath("db.sqlite")));
        MigrationManager manager(root.path());
        QVERIFY(!manager.applyMigrations());
        QVERIFY2(manager.lastError().contains("002_broken.sql"), qPrintable(manager.lastError()));
        QVERIFY2(manager.lastError().contains("syntax error"), qPrintable(manager.lastError()));

        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT COUNT(*) FROM sqlite_master WHERE name='must_rollback'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void semicolonsInCommentsDontSplitStatements() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QFile migration(root.filePath("002_comments.sql"));
        QVERIFY(migration.open(QIODevice::WriteOnly | QIODevice::Text));
        migration.write("-- Notes; with semicolons\n"
                        "CREATE TABLE notes (\n"
                        "    id INTEGER,  -- Key; never reused\n"
                        "    text TEXT DEFAULT '--; not a comment'\n"
                        ");\n");
        migration.close();

        QVERIFY(DatabaseManager::instance().openDatabase(root.filePath("db.sqlite")));
        MigrationManager manager(root.path());
        manager.applyMigrations();                       // Reports the missing app tables; the script itself ran
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("INSERT INTO notes (id) VALUES (1)"));
        QVERIFY(query.exec("SELECT text FROM notes") && query.next());
        QCOMPARE(query.value(0).toString(), QString("--; not a comment"));
    }

    void partiallyInitializedVersionOneCanRecover() {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(QFile::copy(BuiltInMigrations + "/001_initial_schema.sql",
                            root.filePath("001_initial_schema.sql")));
        QVERIFY(DatabaseManager::instance().openDatabase(root.filePath("db.sqlite")));

        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("CREATE TABLE account_types (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE)"));
        QVERIFY(query.exec("INSERT INTO account_types(name) VALUES ('Checking')"));

        MigrationManager manager(root.path());
        QVERIFY(manager.applyMigrations());
        QVERIFY(query.exec("SELECT COUNT(*) FROM schema_migrations WHERE version=1"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        QVERIFY(query.exec("SELECT COUNT(*) FROM account_types WHERE name='Checking'"));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
    }
};

QTEST_MAIN(TestMigrationManager)
#include "test_migrationmanager.moc"
