#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>

#include "account.h"
#include "csvimport.h"
#include "dialog/deleteaccountdialog.h"
#include "tests/test_db_fixture.h"

class TestDeleteAccount : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;
    int savings = -1;

    // A rule with these conditions: "account" on the given account, plus one on the payee if wanted
    static int addRule(int accountId, bool matchAll, bool alsoPayee) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("INSERT INTO rules (user_id, position, match_all, new_payee) VALUES (1, 0, :all, 'Renamed')");
        query.bindValue(":all", matchAll ? 1 : 0);
        if (!query.exec()) {
            return -1;
        }
        const int ruleId = query.lastInsertId().toInt();
        query.prepare("INSERT INTO rule_conditions (rule_id, field, operator, value) VALUES (:rule, 'account', 'is', :value)");
        query.bindValue(":rule", ruleId);
        query.bindValue(":value", QString::number(accountId));
        query.exec();
        if (alsoPayee) {
            query.prepare("INSERT INTO rule_conditions (rule_id, field, operator, value) VALUES (:rule, 'payee', 'contains', 'Shop')");
            query.bindValue(":rule", ruleId);
            query.exec();
        }
        return ruleId;
    }

    static int count(const QString& sql, int id) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare(sql);
        query.bindValue(":id", id);
        return query.exec() && query.next() ? query.value(0).toInt() : -1;
    }
    static bool ruleExists(int ruleId) { return count("SELECT COUNT(*) FROM rules WHERE id = :id", ruleId) == 1; }
    static int conditions(int ruleId) { return count("SELECT COUNT(*) FROM rule_conditions WHERE rule_id = :id", ruleId); }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 5000);
        savings = AccountDAO::createAccount(1, "Tagesgeld", "Savings", 1000);
    }
    void cleanup() { db.close(); }

    void movingKeepsTheTransactionsAndAddsUpTheBalance() {
        QVERIFY(AccountDAO().deleteAccountMovingTo(giro, savings));
        AccountDAO dao;
        QCOMPARE(dao.getAccountById(giro).getAccountId(), -1);
        QCOMPARE(dao.getAccountById(savings).getBalanceCents(), Cents(6000));
        QCOMPARE(dao.transactionCount(savings), 2);
    }

    void movingPointsRulesAtTheTarget() {
        const int rule = addRule(giro, true, true);
        QCOMPARE(AccountDAO().ruleCount(giro), 1);
        QVERIFY(AccountDAO().deleteAccountMovingTo(giro, savings));
        QCOMPARE(AccountDAO().ruleCount(savings), 1);
        QCOMPARE(conditions(rule), 2);
    }

    void movingNeedsAnotherAccountOfTheSameUser() {
        AccountDAO dao;
        QVERIFY(!dao.deleteAccountMovingTo(giro, giro));
        QVERIFY(!dao.deleteAccountMovingTo(giro, 999999));
        QVERIFY(!dao.deleteAccountMovingTo(999999, giro));
        QCOMPARE(dao.getAccountById(giro).getBalanceCents(), Cents(5000));
    }

    void deletingRemovesARuleThatNeedsTheAccount() {
        const int rule = addRule(giro, true, true);       // Without the account it would match every "Shop"
        QVERIFY(AccountDAO().deleteAccount(giro));
        QVERIFY(!ruleExists(rule));
    }

    void deletingLeavesAnyMatchRuleItsOtherConditions() {
        const int rule = addRule(giro, false, true);
        QVERIFY(AccountDAO().deleteAccount(giro));
        QVERIFY(ruleExists(rule));
        QCOMPARE(conditions(rule), 1);
    }

    void deletingRemovesAnAnyMatchRuleWithNothingLeft() {
        const int rule = addRule(giro, false, false);
        QVERIFY(AccountDAO().deleteAccount(giro));
        QVERIFY(!ruleExists(rule));
    }

    void deletingLeavesOtherAccountsRulesAlone() {
        const int rule = addRule(savings, true, true);
        QVERIFY(AccountDAO().deleteAccount(giro));
        QVERIFY(ruleExists(rule));
        QCOMPARE(conditions(rule), 2);
    }

    void aNewAccountDoesntInheritRules() {                // SQLite can give it the deleted account's id
        addRule(savings, true, false);
        QVERIFY(AccountDAO().deleteAccount(savings));
        const int next = AccountDAO::createAccount(1, "Neu", "Savings", 0);
        QCOMPARE(AccountDAO().ruleCount(next), 0);
    }

    void dialogMovesByDefault() {
        DeleteAccountDialog dialog(1, giro);
        QVERIFY(dialog.findChild<QRadioButton*>("moveTransactionsOption")->isChecked());
        QCOMPARE(dialog.findChild<QComboBox*>("targetAccountCombo")->currentData().toInt(), savings);
        auto* button = dialog.findChild<QPushButton*>("deleteAccountButton");
        QCOMPARE(button->text(), QString("Move 1 and delete Girokonto"));
        QVERIFY(!button->isDefault());                    // Enter mustn't delete

        button->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(AccountDAO().getAccountById(savings).getBalanceCents(), Cents(6000));
    }

    void dialogCanDeleteTheTransactions() {
        DeleteAccountDialog dialog(1, giro);
        dialog.findChild<QRadioButton*>("deleteTransactionsOption")->setChecked(true);
        auto* button = dialog.findChild<QPushButton*>("deleteAccountButton");
        QCOMPARE(button->text(), QString("Delete Girokonto and 1 transaction"));
        QVERIFY(!dialog.findChild<QComboBox*>("targetAccountCombo")->isEnabled());

        button->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(AccountDAO().getAccountById(giro).getAccountId(), -1);
        QCOMPARE(AccountDAO().getAccountById(savings).getBalanceCents(), Cents(1000));
    }

    void dialogForTheLastAccountCanOnlyDelete() {
        QVERIFY(AccountDAO().deleteAccount(savings));
        DeleteAccountDialog dialog(1, giro);
        QVERIFY(!dialog.findChild<QRadioButton*>("moveTransactionsOption")->isEnabled());
        QVERIFY(dialog.findChild<QRadioButton*>("deleteTransactionsOption")->isChecked());
    }

    void dialogWithoutTransactionsHasNothingToChoose() {
        const int empty = AccountDAO::createAccount(1, "Leer", "Savings", 0);
        DeleteAccountDialog dialog(1, empty);
        QCOMPARE(dialog.findChild<QPushButton*>("deleteAccountButton")->text(), QString("Delete Leer"));
        dialog.findChild<QPushButton*>("deleteAccountButton")->click();
        QCOMPARE(AccountDAO().getAccountById(empty).getAccountId(), -1);
        QCOMPARE(AccountDAO().transactionCount(savings), 1);
    }

    // --- Hostile input ---

    void movedTransactionsAreStillRecognisedByTheNextImport() {
        using namespace CsvImport;
        const QList<QStringList> rows = {{"2026-01-02", "-3.20", "Cafe", "Coffee"}};
        const Mapping mapping = {{FieldDate, 0}, {FieldAmount, 1}, {FieldPayee, 2}, {FieldDescription, 3}};
        Options options;
        options.accountId = giro;
        QCOMPARE(run(rows, mapping, options, {}).imported, 1);
        QVERIFY(AccountDAO().deleteAccountMovingTo(giro, savings));

        // The bank's overlapping export, now into the account they were moved to
        options.accountId = savings;
        const Result again = run(rows, mapping, options, {});
        QCOMPARE(again.imported, 0);
        QCOMPARE(again.duplicates, 1);
    }
};

QTEST_MAIN(TestDeleteAccount)
#include "test_deleteaccount.moc"
