#include <QtTest>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

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

    void changesTheTransactionsWhenConfirmed() {
        const int markt = add("REWE Markt", 3240);
        const int city = add("REWE City", 1200, "Entertainment");
        ApplyRulesDialog dialog(1);
        dialog.findChild<QPushButton*>("applyRulesButton")->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(TransactionDAO().getTransactionById(markt).getCategoryId(), categoryId("Groceries"));
        QCOMPARE(TransactionDAO().getTransactionById(city).getCategoryId(), categoryId("Entertainment"));   // Kept
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
