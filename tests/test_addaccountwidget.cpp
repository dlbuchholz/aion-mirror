#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include "account.h"
#include "balance.h"
#include "tests/test_db_fixture.h"
#include "widgets/addaccountwidget.h"

class TestAddAccountWidget : public QObject {
    Q_OBJECT
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
    }
    void cleanup() { db.close(); }

    void offersTheAccountTypes() {
        AddAccountWidget form;
        auto* types = form.findChild<QComboBox*>("accountTypeComboBox");
        QCOMPARE(types->count(), AccountDAO().getAccountTypes().size());
        QVERIFY(types->findText("Savings") >= 0);
    }

    void createsTheAccountWithItsStartingBalance() {
        AddAccountWidget form;
        form.setUserId(1);
        QSignalSpy created(&form, &AddAccountWidget::accountCreated);
        form.findChild<QLineEdit*>("accountNameInput")->setText("  Tagesgeld  ");
        form.findChild<QComboBox*>("accountTypeComboBox")->setCurrentText("Savings");
        form.findChild<QDoubleSpinBox*>("initialBalanceInput")->setValue(1500.25);
        form.findChild<QAbstractButton*>("createAccountButton")->click();

        QCOMPARE(created.count(), 1);
        const QList<Account> accounts = AccountDAO().getAllAccountsForUser(1);
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(accounts.first().getName(), QString("Tagesgeld"));
        QCOMPARE(accounts.first().getType(), QString("Savings"));
        QCOMPARE(accounts.first().getBalanceCents(), Cents(150025));

        // Ready for the next one
        QVERIFY(form.findChild<QLineEdit*>("accountNameInput")->text().isEmpty());
        QCOMPARE(form.findChild<QDoubleSpinBox*>("initialBalanceInput")->value(), 0.0);
    }

    void accountWithoutStartingBalanceHasNoTransaction() {
        AddAccountWidget form;
        form.setUserId(1);
        form.findChild<QLineEdit*>("accountNameInput")->setText("Giro");
        form.findChild<QAbstractButton*>("createAccountButton")->click();
        const int accountId = AccountDAO().getAllAccountsForUser(1).first().getAccountId();
        QVERIFY(TransactionDAO().getTransactionsForAccount(accountId).isEmpty());
    }
};

QTEST_MAIN(TestAddAccountWidget)
#include "test_addaccountwidget.moc"
