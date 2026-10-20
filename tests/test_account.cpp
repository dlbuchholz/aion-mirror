#include <QtTest>

#include "account.h"
#include "transaction.h"

#include "tests/test_db_fixture.h"

class TestAccountDAO : public QObject {
    Q_OBJECT

private:
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void testBalancesArePerAccount() {
        AccountDAO dao;
        TransactionDAO tx;

        const int a1 = dao.createAccount(1, "A1", "Checking", 10000);
        const int a2 = dao.createAccount(1, "A2", "Checking", 5000);
        QVERIFY(a1 > 0 && a2 > 0);

        // expense only on a1
        Transaction t(-1, a1, "", 1000, QDate::currentDate(), "x", 0, "expense");
        QVERIFY(tx.addTransaction(t) > 0);

        QCOMPARE(dao.getAccountById(a1).getBalanceCents(), Cents(9000));
        QCOMPARE(dao.getAccountById(a2).getBalanceCents(), Cents(5000));
    }

    void testCreateAccount() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "Test", "Checking", 10000);
        QVERIFY2(accountId > 0, "createAccount returned invalid account id");

        const Account acc = dao.getAccountById(accountId);
        QCOMPARE(acc.getName(), QString("Test"));
    }

    void testCreateAccount_invalidType_rollsBack() {
        AccountDAO dao;

        const int accountId = dao.createAccount(1, "ShouldNotExist", "NoSuchType", 1000);
        QCOMPARE(accountId, -1);

        // Ensure account wasn't inserted
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT COUNT(*) FROM accounts WHERE user_id = :uid AND name = :name");
        q.bindValue(":uid", 1);
        q.bindValue(":name", "ShouldNotExist");
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void testCreateAccount_emptyName_rejected() {
        AccountDAO dao;
        const int id = dao.createAccount(1, "", "Checking", 0);
        QCOMPARE(id, -1);
    }

    void testCreateAccount_whitespaceName_rejected() {
        AccountDAO dao;
        const int id = dao.createAccount(1, "   ", "Checking", 0);
        QCOMPARE(id, -1);
    }

    void testCreateAccount_emptyType_rejected() {
        AccountDAO dao;
        const int id = dao.createAccount(1, "SomeName", "", 0);
        QCOMPARE(id, -1);
    }

    void testCreateAccount_veryLongName_roundTrips() {
        AccountDAO dao;
        const QString longName(1024, 'A');

        const int id = dao.createAccount(1, longName, "Checking", 0);
        QVERIFY(id > 0);

        const Account acc = dao.getAccountById(id);
        QCOMPARE(acc.getName(), longName);
    }

    void testCreateAccount_negativeInitialBalance_createsTransactionAndBalance() {
        AccountDAO dao;
        const int id = dao.createAccount(1, "Neg", "Checking", -5000);
        QVERIFY(id > 0);

        const Account acc = dao.getAccountById(id);
        // A negative start is stored as an expense with a positive amount
        QCOMPARE(acc.getBalanceCents(), Cents(-5000));
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec(QString("SELECT amount, type FROM transactions WHERE account_id = %1").arg(id)) && q.next());
        QCOMPARE(q.value(0).toLongLong(), 5000);
        QCOMPARE(q.value(1).toString(), QString("expense"));
    }

    void testCreateAccount_zeroInitialBalance_createsNoTransaction() {
        AccountDAO dao;

        const int accountId = dao.createAccount(1, "ZeroBalance", "Checking", 0);
        QVERIFY(accountId > 0);

        // balance should be 0 via view
        const Account acc = dao.getAccountById(accountId);
        QCOMPARE(acc.getBalanceCents(), Cents(0));

        // no transactions should exist
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT COUNT(*) FROM transactions WHERE account_id = :aid");
        q.bindValue(":aid", accountId);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void testCreateAccount_initialDepositTransactionInserted() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "InitTx", "Checking", 10000);
        QVERIFY(accountId > 0);

        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT amount, type, description FROM transactions WHERE account_id = :aid");
        q.bindValue(":aid", accountId);
        QVERIFY(q.exec());
        QVERIFY(q.next());

        QCOMPARE(q.value(0).toLongLong(), 10000);
        QCOMPARE(q.value(1).toString(), QString("income"));
        QCOMPARE(q.value(2).toString(), QString("Initial deposit"));
    }

    void testCreateAccount_initialTransactionIsInInitialBalanceCategory() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "InitCat", "Checking", -2500);
        QVERIFY(accountId > 0);

        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT c.name FROM transactions t JOIN categories c ON c.id = t.category_id WHERE t.account_id = :aid");
        q.bindValue(":aid", accountId);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString("Initial balance"));
    }

    void testCreateAccount_withoutInitialBalanceCategory_hasNoCategory() {
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("DELETE FROM categories WHERE name = 'Initial balance'"));

        AccountDAO dao;
        const int accountId = dao.createAccount(1, "NoInitCat", "Checking", 10000);
        QVERIFY(accountId > 0);
        QCOMPARE(dao.getAccountById(accountId).getBalanceCents(), Cents(10000));

        q.prepare("SELECT category_id FROM transactions WHERE account_id = :aid");
        q.bindValue(":aid", accountId);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QVERIFY(q.value(0).isNull());
    }

    void testCreateAccount_twice_returnsDistinctIds_andBothPersist() {
        AccountDAO dao;

        const int id1 = dao.createAccount(1, "AccountOne", "Checking", 0);
        QVERIFY2(id1 > 0, "First createAccount returned invalid id");

        const int id2 = dao.createAccount(1, "AccountTwo", "Checking", 0);
        QVERIFY2(id2 > 0, "Second createAccount returned invalid id");

        QVERIFY2(id1 != id2, "Expected distinct account IDs from two createAccount() calls");

        const Account a1 = dao.getAccountById(id1);
        QCOMPARE(a1.getAccountId(), id1);
        QCOMPARE(a1.getName(), QString("AccountOne"));

        const Account a2 = dao.getAccountById(id2);
        QCOMPARE(a2.getAccountId(), id2);
        QCOMPARE(a2.getName(), QString("AccountTwo"));
    }

    void testGetAccountTypes_containsSeededTypes() {
        AccountDAO dao;
        const auto types = dao.getAccountTypes(1);
        QVERIFY2(types.contains("Checking"), "Expected seeded account type 'Checking' to exist");
    }

    void testAddAccountType_duplicateName_fails() {
       AccountDAO dao;

       const int id1 = dao.addAccountType(1, "DupType");
       QVERIFY(id1 > 0);

       const int id2 = dao.addAccountType(1, "DupType");
       QCOMPARE(id2, -1);
    }

    void testAccountTypeCrud_addEditDelete() {
        AccountDAO dao;

        const int id = dao.addAccountType(1, "TestType");
        QVERIFY2(id > 0, "addAccountType should return a positive id");

        QVERIFY(dao.editAccountType(1, id, "TestTypeRenamed"));

        const auto all = dao.getAllAccountTypes(1);
        bool found = false;
        for (const auto& pair : all) {
            if (pair.first.toInt() == id && pair.second == "TestTypeRenamed") {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Edited account type not found in getAllAccountTypes(1)");

        QVERIFY(dao.deleteAccountType(1, id));
    }

    void testEditAccountType_nonexistentId_returnsFalse() {
        AccountDAO dao;
        QCOMPARE(dao.editAccountType(1, 999999, "DoesNotMatter"), false);
    }

    void testGetAllAccountsForUser_filtersByUser() {
        // Seed another user
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(UserDAO().createUser(User(2, "testuser2", "Test User 2")));

        AccountDAO dao;
        const int a1 = dao.createAccount(1, "User1Account", "Checking", 0);
        QVERIFY(a1 > 0);
        const int a2 = dao.createAccount(2, "User2Account", "Checking", 0);
        QVERIFY(a2 > 0);

        const auto user1Accounts = dao.getAllAccountsForUser(1);
        QVERIFY(!user1Accounts.isEmpty());
        for (const auto& acc : user1Accounts) {
            QCOMPARE(acc.getName() == "User2Account", false);
        }
    }

    void testGetAllAccountsForUser_noAccounts_returnsEmpty() {
        AccountDAO dao;
        const auto accounts = dao.getAllAccountsForUser(1);
        QVERIFY(accounts.isEmpty());
    }

    void testGetAllAccountTypes_containsCheckingWithNumericId() {
        AccountDAO dao;
        const auto all = dao.getAllAccountTypes(1);

        QVERIFY2(!all.isEmpty(), "Expected seeded account types from migrations");

        bool foundChecking = false;
        for (const auto& p : all) {
            bool ok = false;
            p.first.toInt(&ok);
            QVERIFY2(ok, "Expected account_types.id to be numeric");

            if (p.second == "Checking") {
                foundChecking = true;
            }
        }
        QVERIFY2(foundChecking, "Expected seeded account type 'Checking'");
    }

    void testDeleteAccount_cascadesTransactions() {
        AccountDAO dao;

        const int accountId = dao.createAccount(1, "CascadeTest", "Checking", 10000);
        QVERIFY(accountId > 0);

        // There should be at least one transaction (initial deposit)
        {
            QSqlQuery q(DatabaseManager::instance().database());
            q.prepare("SELECT COUNT(*) FROM transactions WHERE account_id = :aid");
            q.bindValue(":aid", accountId);
            QVERIFY(q.exec());
            QVERIFY(q.next());
            QVERIFY2(q.value(0).toInt() >= 1, "Expected initial transaction to exist");
        }

        QVERIFY(dao.deleteAccount(accountId));

        // Transactions should be gone (ON DELETE CASCADE)
        {
            QSqlQuery q(DatabaseManager::instance().database());
            q.prepare("SELECT COUNT(*) FROM transactions WHERE account_id = :aid");
            q.bindValue(":aid", accountId);
            QVERIFY(q.exec());
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);
        }
    }

    void testDeleteAccountType_removesRow() {
        AccountDAO dao;
        const int id = dao.addAccountType(1, "TempDeleteMe");
        QVERIFY(id > 0);

        QVERIFY(dao.deleteAccountType(1, id));

        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT COUNT(*) FROM account_types WHERE id = :id");
        q.bindValue(":id", id);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void testDeleteAccountType_inUse_isRestricted() {
        AccountDAO dao;

        // Create an account that uses a seeded type ("Checking")
        const int accountId = dao.createAccount(1, "UsesChecking", "Checking", 0);
        QVERIFY(accountId > 0);

        // Get Checking type_id
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT id FROM account_types WHERE name = :name");
        q.bindValue(":name", "Checking");
        QVERIFY(q.exec());
        QVERIFY(q.next());
        const int checkingId = q.value(0).toInt();

        // Should fail due to ON DELETE RESTRICT
        const bool deleted = dao.deleteAccountType(1, checkingId);
        QCOMPARE(deleted, false);
    }

    void testDeleteAccountType_inUse_doesNotRemoveRow() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "UsesChecking2", "Checking", 0);
        QVERIFY(accountId > 0);

        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("SELECT id FROM account_types WHERE name='Checking'");
        QVERIFY(q.exec());
        QVERIFY(q.next());
        const int checkingId = q.value(0).toInt();

        QCOMPARE(dao.deleteAccountType(1, checkingId), false);

        q.prepare("SELECT COUNT(*) FROM account_types WHERE id = :id");
        q.bindValue(":id", checkingId);
        QVERIFY(q.exec());
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void testDeleteAccount_nonexistent_returnsFalse() {
        AccountDAO dao;
        QCOMPARE(dao.deleteAccount(999999), false);
    }

    void testDeleteAccount_removesFromBalancesView() {
        AccountDAO dao;
        const int accountId = dao.createAccount(1, "ToDelete", "Checking", 0);
        QVERIFY(accountId > 0);

        QVERIFY(dao.deleteAccount(accountId));

        const Account acc = dao.getAccountById(accountId);
        QCOMPARE(acc.getAccountId(), -1);
    }

    void testGetAccountById_balanceReflectsTransactions() {
        AccountDAO accountDao;
        TransactionDAO txDao;

        const int accountId = accountDao.createAccount(1, "BalanceTest", "Checking", 10000);
        QVERIFY(accountId > 0);

        // Add an expense of 25
        Transaction t(-1, accountId, "", 2500, QDate::currentDate(), "Test expense", 0, "expense");
        const int txId = txDao.addTransaction(t);
        QVERIFY2(txId > 0, "Expected addTransaction to return a positive id");

        const Account acc = accountDao.getAccountById(accountId);
        QCOMPARE(acc.getBalanceCents(), Cents(7500));
    }

    void testGetAccountById_notFound_returnsInvalid() {
            AccountDAO dao;
            const Account acc = dao.getAccountById(999999);
            QCOMPARE(acc.getAccountId(), -1);
            QCOMPARE(acc.getName(), QString(""));
            QCOMPARE(acc.getType(), QString(""));
            QCOMPARE(acc.getBalanceCents(), Cents(0));
        }


    void testUpdateTransaction_refreshesHash() {
        AccountDAO accountDao;
        TransactionDAO transactionDao;
        const int accountId = accountDao.createAccount(1, "HashTest", "Checking", 0);
        QVERIFY(accountId > 0);

        Transaction transaction(-1, accountId, "Before", 1000, QDate(2026, 1, 1), "Old", 0, "expense");
        const int transactionId = transactionDao.addTransaction(transaction);
        QVERIFY(transactionId > 0);

        transaction.setTransactionId(transactionId);
        transaction.setPayee("After");
        transaction.setDescription("New");
        QVERIFY(transactionDao.updateTransaction(transaction));

        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT hash FROM transactions WHERE id = :id");
        query.bindValue(":id", transactionId);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), transaction.calculateHash());
    }

    void testUpdateTransaction_rejectsTemporaryId() {
        Transaction transaction;
        transaction.setTransactionId(-1);
        TransactionDAO dao;
        QVERIFY(!dao.updateTransaction(transaction));
    }

    void testUpdateTransaction_rejectsUnknownId() {
        Transaction transaction;
        transaction.setTransactionId(999999);
        transaction.setAccountId(1);
        TransactionDAO dao;
        QVERIFY(!dao.updateTransaction(transaction));
    }

    void testDeleteTransaction_unknownIdReturnsFalse() {
        TransactionDAO dao;
        QVERIFY(!dao.deleteTransaction(999999));
    }
};

QTEST_MAIN(TestAccountDAO)
#include "test_account.moc"
