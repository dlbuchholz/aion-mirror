#include <QtTest>
#include "account.h"
#include "transaction.h"
#include "db/databasemanager.h"
 
class TestAccountDAO : public QObject {
    Q_OBJECT
 
private slots:
    void initTestCase() {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(":memory:");
        db.open();
        DatabaseManager::instance().setDatabase(db);
 
        QSqlQuery query(db);
        query.exec("CREATE TABLE account_types (id INTEGER PRIMARY KEY, name TEXT)");
        query.exec("CREATE TABLE accounts (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, name TEXT, type_id INTEGER)");
        query.exec("CREATE TABLE transactions (id INTEGER PRIMARY KEY AUTOINCREMENT, account_id INTEGER, amount REAL, date TEXT, description TEXT, category TEXT, type TEXT)");
        query.exec("INSERT INTO account_types (id, name) VALUES (1, 'Checking')");
    }
 
    void testAccountCreation() {
        AccountDAO dao;
        int accountId = dao.createAccount(1, "Test", "Checking", 100.0);
        QVERIFY(accountId > 0);
 
        Account acc = dao.getAccountById(accountId);
        QCOMPARE(acc.getName(), QString("Test"));
    }
};
 
QTEST_MAIN(TestAccountDAO)
#include "test_account.moc"
