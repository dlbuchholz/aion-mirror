#include <QtTest>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/migrationmanager.h"

class TestMigrationManager : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { DatabaseManager::instance().closeDatabase(); }

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
        QVERIFY(QFile::copy(QString(SQL_MIGRATIONS_DIR) + "/001_initial_schema.sql",
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
