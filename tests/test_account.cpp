#include <QtTest>

#include "account.h"
#include "transaction.h"

#include "tests/test_db_fixture.h"

class TestAccountDAO : public QObject {
    Q_OBJECT

private:
    TestDbFixture db;

private slots:
    void initTestCase() {
        db.init();
        db.seedDefaultUser();
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
