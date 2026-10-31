#include <QtTest>

#include "account.h"
#include "tests/test_db_fixture.h"
#include "transaction.h"

// Logs can end up in places the user doesn't control, so they stay free of the user data
class TestLogging : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int accountId = -1;

    static QStringList& logged() {
        static QStringList messages;
        return messages;
    }
    static void capture(QtMsgType, const QMessageLogContext&, const QString& message) {
        logged().append(message);
    }
    static Transaction secretTransaction(int id, int account) {
        return Transaction(id, account, "SecretPayee", 1234567, QDate(2026, 10, 1), "SecretDescription", 0, "expense");
    }
    static void verifyNoSecrets() {
        const QString all = logged().join('\n');
        QVERIFY2(!all.contains("SecretPayee"), qPrintable(all));
        QVERIFY2(!all.contains("SecretDescription"), qPrintable(all));
        QVERIFY2(!all.contains("12345.67"), qPrintable(all));
        QVERIFY2(!all.contains("1234567"), qPrintable(all));
    }

private slots:
    void initTestCase() { db.init(); }

    void init() {
        db.yankDb();
        db.seedDefaultUser();
        accountId = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        logged().clear();
    }
    void cleanup() { db.close(); }

    void savingATransactionLogsNoneOfIt() {
        QtMessageHandler previous = qInstallMessageHandler(capture);
        TransactionDAO transactions;
        const int id = transactions.addTransaction(secretTransaction(-1, accountId));
        const bool updated = id > 0 && transactions.updateTransaction(secretTransaction(id, accountId));
        qInstallMessageHandler(previous);

        QVERIFY(updated);
        verifyNoSecrets();
    }

    void aFailedSaveLogsTheErrorButNotTheValues() {
        TransactionDAO transactions;
        const int id = transactions.addTransaction(secretTransaction(-1, accountId));
        QVERIFY(id > 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("DROP TABLE transactions"));

        QtMessageHandler previous = qInstallMessageHandler(capture);
        const bool updated = transactions.updateTransaction(secretTransaction(id, accountId));
        qInstallMessageHandler(previous);

        QVERIFY(!updated);
        QVERIFY2(logged().join('\n').contains("TransactionDAO"), "The failure should be logged, in every build");
        verifyNoSecrets();
    }
};

QTEST_MAIN(TestLogging)
#include "test_logging.moc"
