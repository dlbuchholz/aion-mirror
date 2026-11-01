#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlQuery>
#include <QTimer>

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
        form.setUserId(1);
        auto* types = form.findChild<QComboBox*>("accountTypeComboBox");
        QCOMPARE(types->count(), AccountDAO().getAccountTypes(1).size());
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

    // Clicks Create and answers the message box that blocks it; returns what it said
    static QString messageOnCreate(AddAccountWidget& form) {
        QString said;
        QTimer::singleShot(0, qApp, [&said]() {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                said = box->text();
                box->accept();
            }
        });
        form.findChild<QAbstractButton*>("createAccountButton")->click();
        QCoreApplication::processEvents();               // The queued call, if no box opened
        return said;
    }

    void withoutAUserNothingIsCreated() {
        AddAccountWidget form;
        form.findChild<QLineEdit*>("accountNameInput")->setText("Giro");
        QVERIFY(messageOnCreate(form).contains("User ID is not set"));
        QVERIFY(AccountDAO().getAllAccountsForUser(1).isEmpty());
    }

    void aNameOfOnlySpacesIsRefused() {
        AddAccountWidget form;
        form.setUserId(1);
        QSignalSpy created(&form, &AddAccountWidget::accountCreated);
        form.findChild<QLineEdit*>("accountNameInput")->setText("   ");
        QVERIFY(messageOnCreate(form).contains("valid account name"));
        QCOMPARE(created.count(), 0);
        QVERIFY(AccountDAO().getAllAccountsForUser(1).isEmpty());
    }

    void anAccountNeedsAType() {
        AddAccountWidget form;
        form.setUserId(1);
        form.findChild<QComboBox*>("accountTypeComboBox")->clear();
        form.findChild<QLineEdit*>("accountNameInput")->setText("Giro");
        QVERIFY(messageOnCreate(form).contains("select an account type"));
        QVERIFY(AccountDAO().getAllAccountsForUser(1).isEmpty());
    }

    void anAccountThatCantBeSavedKeepsTheForm() {
        AddAccountWidget form;
        form.setUserId(1);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("DELETE FROM account_types"));   // The offered type is gone by now
        QSignalSpy created(&form, &AddAccountWidget::accountCreated);
        form.findChild<QLineEdit*>("accountNameInput")->setText("Giro");
        QVERIFY(messageOnCreate(form).contains("Failed to create"));
        QCOMPARE(created.count(), 0);
        QCOMPARE(form.findChild<QLineEdit*>("accountNameInput")->text(), QString("Giro"));
    }
};

QTEST_MAIN(TestAddAccountWidget)
#include "test_addaccountwidget.moc"
