#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include "account.h"
#include "transaction.h"
#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "tests/test_db_utils.h"

class TestAccountDAO : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tempDir;

private slots:
    void initTestCase() {
        tempDir = std::make_unique<QTemporaryDir>();
        QVERIFY2(tempDir->isValid(), "Failed to create temporary directory for test database");

        const QString dbPath = QDir(tempDir->path()).filePath("test.db");

        // Open DB (like our client)
        QVERIFY2(DatabaseManager::instance().openDatabase(dbPath),
                 "Failed to open test database");

        // Apply migrations (like our client)
        MigrationManager migrationManager(SQL_MIGRATIONS_DIR);
        QVERIFY2(migrationManager.applyMigrations(),
                 "Failed to apply migrations to test database");

        // Minimal seed data the test depends on
        QSqlQuery q(DatabaseManager::instance().database());
        execOrFail(q, "INSERT INTO users (id, username, name) VALUES (1, 'testuser', 'Test User')");
    }

    void testAccountCreation() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "Test", "Checking", 100.0);
        QVERIFY2(accountId > 0, "createAccount returned invalid account id");

        const Account acc = dao.getAccountById(accountId);
        QCOMPARE(acc.getName(), QString("Test"));
    }
};
 
QTEST_MAIN(TestAccountDAO)
#include "test_account.moc"
