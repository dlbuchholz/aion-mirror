#include <QtTest>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>

#include "account.h"
#include "budget.h"
#include "category.h"
#include "csvimport.h"
#include "db/migrationmanager.h"
#include "dialog/renamedpayeesdialog.h"
#include "icons.h"
#include "rule.h"
#include "settings.h"
#include "systeminfo.h"
#include "tests/test_db_fixture.h"
#include "transaction.h"
#include "user.h"

// What happens when the database itself fails: a transaction that can't start (SQLite refuses a
// second one inside the first), a commit that fails, and tables that can't be read
class TestDatabaseFailures : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;

    static QSqlDatabase database() { return DatabaseManager::instance().database(); }

    static CsvImport::Result import() {
        CsvImport::Options options;
        options.accountId = 1;
        options.decimalSeparator = ',';
        const CsvImport::Mapping mapping = {{CsvImport::FieldDate, 0}, {CsvImport::FieldAmount, 1}, {CsvImport::FieldPayee, 2}};
        return CsvImport::run({{"10.10.2026", "-1,00", "Shop"}}, mapping, options, {});
    }

    int addExpense(const char* payee) {
        return TransactionDAO().addTransaction(Transaction(-1, giro, payee, 990, QDate(2026, 10, 4), "", 0, "expense"));
    }

    // Clicks apply in the dialog and returns the warning it shows
    QString applyRenames(int transactionId) {
        RenamedPayeesDialog dialog({TransactionDAO().getTransactionById(transactionId)}, "Kiosk", "Kiosk Nord", -1);
        QString said;
        QTimer::singleShot(0, qApp, [&said]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            said = box->text();
            box->accept();
        });
        dialog.findChild<QPushButton*>("renamedPayeesApplyButton")->click();
        return said;
    }

    static Rule ruleFor(const QString& payee) {
        Rule rule;
        rule.newPayee = "Renamed";
        rule.conditions = {{RuleCondition::Field::Payee, RuleCondition::Operator::Contains, payee}};
        return rule;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QCOMPARE(giro, 1);
    }
    void cleanup() { db.close(); }

    void whatCantStartATransactionChangesNothing() {
        const int transactionId = addExpense("Kiosk");
        const int otherAccount = AccountDAO::createAccount(1, "Other", "Savings", 0);
        Rule rule = ruleFor("Kiosk");
        rule.id = RuleDAO(1).addRule(rule);
        QVERIFY(rule.id > 0);
        const int category = CategoryDAO(1).addCategory(Category(-1, "Unused", "expense"));
        QVERIFY(category > 0);

        QVERIFY(database().transaction());
        QCOMPARE(AccountDAO::createAccount(1, "New", "Savings", 0), -1);
        QVERIFY(!AccountDAO().deleteAccount(otherAccount));
        QVERIFY(!AccountDAO().deleteAccountMovingTo(otherAccount, giro));
        QVERIFY(!CategoryDAO(1).deleteCategory(category));
        QVERIFY(!Rules::apply({}));
        QCOMPARE(RuleDAO(1).addRule(ruleFor("Other")), -1);
        QVERIFY(!RuleDAO(1).updateRule(rule));
        QVERIFY(!RuleDAO(1).setOrder({rule.id}));
        QVERIFY(!TransactionDAO().updateTransactions({}));
        QVERIFY(!TransactionDAO().deleteTransactions({transactionId}));
        QVERIFY(!UserDAO().createUser(User(2, "other", "Other")));
        QCOMPARE(applyRenames(transactionId), QString("The names couldn't be changed."));

        const CsvImport::Result result = import();
        QCOMPARE(result.errors, QStringList({"Failed to start database transaction"}));
        QCOMPARE(result.imported, 0);
        QCOMPARE(result.failed, 1);
        database().rollback();
    }

    void migrationsNeedTheirOwnTransaction() {
        db.close();
        QTemporaryDir dir;
        QVERIFY(DatabaseManager::instance().openDatabase(dir.filePath("empty.db")));
        QVERIFY(database().transaction());
        MigrationManager migrations(BuiltInMigrations);
        QVERIFY(!migrations.applyMigrations());
        database().rollback();
        db.close();
        db.yankDb();
    }

    void aFailedCommitIsReported() {
        QVERIFY(TestDbFixture::failCommitAfter("INSERT", "accounts"));
        QCOMPARE(AccountDAO::createAccount(1, "New", "Savings", 0), -1);
    }

    void anImportThatCantCommitIsUndone() {
        QVERIFY(TestDbFixture::failCommitAfter("INSERT", "transactions"));
        const CsvImport::Result result = import();
        QCOMPARE(result.errors, QStringList({"Failed to commit changes to database"}));
        QCOMPARE(result.imported, 0);
        QCOMPARE(result.failed, 1);
        QVERIFY(TransactionDAO().getTransactionsForAccount(giro).isEmpty());
    }

    void renamesThatCantCommitAreSaid() {
        const int transactionId = addExpense("Kiosk");
        QVERIFY(TestDbFixture::failCommitAfter("UPDATE", "transactions"));
        QCOMPARE(applyRenames(transactionId), QString("The names couldn't be changed. Nothing was changed."));
    }

    void aFailedCommitDoesntBlockWhatComesNext() {
        const auto nothingOpen = []() { return database().transaction() && database().rollback(); };
        const int transactionId = addExpense("Kiosk");
        const int otherAccount = AccountDAO::createAccount(1, "Other", "Savings", 0);
        Rule rule = ruleFor("Kiosk");
        rule.id = RuleDAO(1).addRule(rule);
        const int unused = CategoryDAO(1).addCategory(Category(-1, "Unused", "expense"));
        const int unusedToo = CategoryDAO(1).addCategory(Category(-1, "Unused too", "expense"));
        TransactionDAO transactions;

        QVERIFY(TestDbFixture::failCommitAfter("INSERT", "accounts"));
        QCOMPARE(AccountDAO::createAccount(1, "New", "Savings", 0), -1);
        QVERIFY(nothingOpen());
        QCOMPARE(AccountDAO().getAllAccountsForUser(1).size(), 2);

        QVERIFY(TestDbFixture::failCommitAfter("DELETE", "accounts"));
        QVERIFY(!AccountDAO().deleteAccount(otherAccount));
        QVERIFY(nothingOpen());
        QVERIFY(!AccountDAO().deleteAccountMovingTo(otherAccount, giro));
        QVERIFY(nothingOpen());
        QCOMPARE(AccountDAO().getAllAccountsForUser(1).size(), 2);

        QVERIFY(TestDbFixture::failCommitAfter("UPDATE", "transactions"));
        Transaction changed = transactions.getTransactionById(transactionId);
        changed.setPayee("Changed");
        QVERIFY(!transactions.updateTransactions({changed}));
        QVERIFY(nothingOpen());
        const QList<RuleChange> changes = Rules::changesFor({rule}, transactions.getTransactionsForAccount(giro), false, {});
        QCOMPARE(changes.size(), 1);
        QVERIFY(!Rules::apply(changes));
        QVERIFY(nothingOpen());
        QCOMPARE(applyRenames(transactionId), QString("The names couldn't be changed. Nothing was changed."));
        QVERIFY(nothingOpen());
        QCOMPARE(transactions.getTransactionById(transactionId).getPayee(), QString("Kiosk"));

        QVERIFY(TestDbFixture::failCommitAfter("DELETE", "transactions"));
        QVERIFY(!transactions.deleteTransactions({transactionId}));
        QVERIFY(nothingOpen());
        QCOMPARE(transactions.getTransactionsForAccount(giro).size(), 1);

        QVERIFY(TestDbFixture::failCommitAfter("INSERT", "budgets"));
        QVERIFY(!BudgetDAO().setMonthlyLimit(1, 4, QDate(2026, 10, 1), 1000));
        QVERIFY(nothingOpen());

        QVERIFY(TestDbFixture::failCommitAfter("DELETE", "categories"));
        QVERIFY(!CategoryDAO(1).deleteCategory(unused));
        QVERIFY(nothingOpen());
        QVERIFY(!CategoryDAO(1).mergeCategory(unused, unusedToo));
        QVERIFY(nothingOpen());

        QVERIFY(TestDbFixture::failCommitAfter("UPDATE", "rules"));
        QVERIFY(!RuleDAO(1).setOrder({rule.id}));
        QVERIFY(nothingOpen());
    }

    void refusedWritesAreReported() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, 4, QDate(2026, 9, 1), 1000));
        QVERIFY(TestDbFixture::refuse("UPDATE", "budgets"));
        QVERIFY(!BudgetDAO().setMonthlyLimit(1, 4, QDate(2026, 10, 1), 2000));

        QVERIFY(TestDbFixture::refuse("UPDATE", "accounts"));
        const QList<QPair<QString, QString>> types = AccountDAO().getAllAccountTypes(1);
        QVERIFY(!AccountDAO().editAccount(giro, "Renamed", types.first().first.toInt()));

        Rule rule = ruleFor("Kiosk");
        rule.id = RuleDAO(1).addRule(rule);
        QVERIFY(TestDbFixture::refuse("DELETE", "rule_conditions"));
        rule.conditions.first().value = "Other";
        QVERIFY(!RuleDAO(1).updateRule(rule));
        QCOMPARE(RuleDAO(1).rules().first().conditions.first().value, QString("Kiosk"));
    }

    void aSettingThatCantBeSavedStaysAsItWas() {
        QSettings& stored = Settings::instance().getQSettings();
        QMap<QString, QVariant> original;
        for (const QString& key : stored.allKeys()) original.insert(key, stored.value(key));

        Settings& settings = Settings::instance();
        settings.setDecimalSeparator(Settings::DecimalSeparator::Point);
        QVERIFY(TestDbFixture::refuse("INSERT", "database_settings"));
        settings.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        QCOMPARE(settings.getDecimalSeparator(), Settings::DecimalSeparator::Point);

        stored.clear();
        for (auto it = original.cbegin(); it != original.cend(); ++it) stored.setValue(it.key(), it.value());
        stored.sync();
    }

    void transactionsThatCantBeReadAreEmpty() {
        const int transactionId = addExpense("Kiosk");
        QVERIFY(TestDbFixture::dropTable("transactions"));
        TransactionDAO dao;
        QVERIFY(dao.getTransactionsForAccount(giro).isEmpty());
        QVERIFY(dao.getShadowedTransactions().isEmpty());
        QVERIFY(!dao.hasShadowedTransactions());
        QVERIFY(dao.getTransactionsRenamedByRule(1).isEmpty());
        QVERIFY(!dao.transactionExistsByHash("hash"));
        QVERIFY(dao.getExistingHashes({"hash"}).isEmpty());
        QCOMPARE(dao.getTransactionById(transactionId).getPayee(), QString());
    }

    void otherTablesThatCantBeRead() {
        QVERIFY(TestDbFixture::dropTable("rule_conditions"));
        QVERIFY(RuleDAO(1).rules().isEmpty());
        QVERIFY(TestDbFixture::dropTable("rules"));
        QVERIFY(RuleDAO(1).rules().isEmpty());
        QVERIFY(TestDbFixture::dropTable("account_types"));
        QVERIFY(AccountDAO().getAllAccountTypes(1).isEmpty());
        QVERIFY(TestDbFixture::dropTable("users"));
        QVERIFY(UserDAO().getAllUsers().isEmpty());
        QVERIFY(TestDbFixture::dropTable("schema_migrations"));
        QVERIFY(SystemInfo::report().contains("unknown"));
    }

    void renamingToTheSameNameChangesNothing() {
        Transaction transaction(-1, giro, "Kiosk", 990, QDate(2026, 10, 4), "", 0, "expense");
        transaction.renamePayee("Kiosk", 5);
        QVERIFY(transaction.getOriginalPayee().isEmpty());
        QCOMPARE(transaction.getRenamedByRuleId(), -1);
    }

    void aMissingIconIsReported() {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is missing from the resources"));
        QVERIFY(!Icons::tinted("no-such-icon", Qt::black, 16, 1.0).isNull());
    }
};

QTEST_MAIN(TestDatabaseFailures)
#include "test_databasefailures.moc"
