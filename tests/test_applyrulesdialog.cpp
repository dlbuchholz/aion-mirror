#include <QtTest>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include "account.h"
#include "category.h"
#include "dialog/applyrulesdialog.h"
#include "rule.h"
#include "tests/test_db_fixture.h"

using Field = RuleCondition::Field;
using Operator = RuleCondition::Operator;

class TestApplyRulesDialog : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giroId = -1;

    static int categoryId(const QString& name) {
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == name) return category.getCategoryId();
        }
        return -1;
    }
    int add(const QString& payee, Cents amount, const QString& category = QString()) {
        return TransactionDAO().addTransaction(Transaction(-1, giroId, payee, amount, QDate(2026, 10, 1), "",
                                                           category.isEmpty() ? 0 : categoryId(category), "expense"));
    }
    static QStringList payees(ApplyRulesDialog& dialog) {
        QStringList result;
        for (QLabel* label : dialog.findChildren<QLabel*>("applyRulesPayee")) result << label->text();
        result.sort();
        return result;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giroId = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        Rule rule;
        rule.categoryId = categoryId("Groceries");
        rule.conditions = {{Field::PayeeOrDescription, Operator::Contains, "REWE"}};
        QVERIFY(RuleDAO(1).addRule(rule) > 0);
    }
    void cleanup() { db.close(); }

    void previewsOnlyWhatWouldChange() {
        add("REWE Markt", 3240);
        add("REWE City", 1200, "Entertainment");          // Has a category
        add("REWE Online", 900, "Groceries");             // Already right
        add("Aldi", 500);                                 // No rule matches

        ApplyRulesDialog dialog(1);
        QCOMPARE(payees(dialog), QStringList({"REWE Markt"}));
        QCOMPARE(dialog.findChild<QPushButton*>("applyRulesButton")->text(), QString("Change 1 transaction"));

        dialog.findChild<QCheckBox*>("onlyUncategorisedCheck")->setChecked(false);
        QCOMPARE(payees(dialog), QStringList({"REWE City", "REWE Markt"}));
        QCOMPARE(dialog.findChild<QPushButton*>("applyRulesButton")->text(), QString("Change 2 transactions"));
    }

    void aLongListShowsOnlyTheFirstOnes() {
        QSqlDatabase database = DatabaseManager::instance().database();
        QVERIFY(database.transaction());
        for (int i = 0; i < 201; ++i) add("REWE Markt", 100);
        QVERIFY(database.commit());
        ApplyRulesDialog dialog(1);
        QCOMPARE(dialog.findChild<QLabel*>("applyRulesSummary")->text(), QString("Showing 200 of 201 transactions."));
        QCOMPARE(dialog.findChildren<QLabel*>("applyRulesPayee").size(), 200);
        QCOMPARE(dialog.findChild<QPushButton*>("applyRulesButton")->text(), QString("Change 201 transactions"));
    }

    void aFailedChangeIsSaidAndChangesNothing() {
        const int markt = add("REWE Markt", 3240);
        QVERIFY(TestDbFixture::refuse("UPDATE", "transactions"));
        ApplyRulesDialog dialog(1);
        QString said;
        QTimer::singleShot(0, qApp, [&said]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            said = box->text();
            box->accept();
        });
        dialog.findChild<QPushButton*>("applyRulesButton")->click();
        QCOMPARE(said, QString("The transactions couldn't be changed. Nothing was changed."));
        QCOMPARE(dialog.result(), 0);
        QCOMPARE(TransactionDAO().getTransactionById(markt).getCategoryId(), 0);
    }

    void changesTheTransactionsWhenConfirmed() {
        const int markt = add("REWE Markt", 3240);
        const int city = add("REWE City", 1200, "Entertainment");
        ApplyRulesDialog dialog(1);
        dialog.findChild<QPushButton*>("applyRulesButton")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(TransactionDAO().getTransactionById(markt).getCategoryId(), categoryId("Groceries"));
        QCOMPARE(TransactionDAO().getTransactionById(city).getCategoryId(), categoryId("Entertainment"));   // Kept
    }

    void previewsAndAppliesRenames() {
        Rule rename;
        rename.newPayee = "Rewe";
        rename.conditions = {RuleCondition(Field::Payee, Operator::StartsWith, "REWE MARKT")};
        QVERIFY(RuleDAO(1).addRule(rename) > 0);
        const int id = add("REWE MARKT GMBH 4711", 3240, "Entertainment");   // Keeps its category

        ApplyRulesDialog dialog(1);
        QCOMPARE(payees(dialog), QStringList({"REWE MARKT GMBH 4711 → Rewe"}));
        dialog.findChild<QPushButton*>("applyRulesButton")->click();
        const Transaction saved = TransactionDAO().getTransactionById(id);
        QCOMPARE(saved.getPayee(), QString("Rewe"));
        QCOMPARE(saved.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));
        QCOMPARE(saved.getCategoryId(), categoryId("Entertainment"));
    }

    void saysSoWhenNothingWouldChange() {
        add("Aldi", 500);
        ApplyRulesDialog dialog(1);
        QVERIFY(payees(dialog).isEmpty());
        QVERIFY(dialog.findChild<QPushButton*>("applyRulesButton")->isHidden());
        QCOMPARE(dialog.findChild<QLabel*>("applyRulesSummary")->text(), QString("Your rules wouldn't change any transaction."));
    }

    void balanceAdjustmentsKeepTheirCategory() {
        // A starting balance whose description happens to match still stays a starting balance
        const int start = TransactionDAO().addTransaction(Transaction(-1, giroId, "", 50000, QDate(2026, 1, 1),
                                                                      "Start REWE card", categoryId(InitialBalanceCategory), "income"));
        ApplyRulesDialog dialog(1);
        dialog.findChild<QCheckBox*>("onlyUncategorisedCheck")->setChecked(false);
        QVERIFY(payees(dialog).isEmpty());
        QCOMPARE(TransactionDAO().getTransactionById(start).getCategoryId(), categoryId(InitialBalanceCategory));
    }
};

QTEST_MAIN(TestApplyRulesDialog)
#include "test_applyrulesdialog.moc"
