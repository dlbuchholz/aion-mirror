#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "account.h"
#include "dialog/editaccountdialog.h"
#include "tests/test_db_fixture.h"
#include "user.h"

class TestEditAccount : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;

    static int typeId(int userId, const QString& name) {
        for (const auto& type : AccountDAO().getAllAccountTypes(userId)) {
            if (type.second == name) {
                return type.first.toInt();
            }
        }
        return -1;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 5000);
        QVERIFY(AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0) > 0);
    }
    void cleanup() { db.close(); }

    void changesNameAndTypeAndKeepsTheBalance() {
        QVERIFY(AccountDAO().editAccount(giro, "  Hausbank  ", typeId(1, "Savings")));
        const Account account = AccountDAO().getAccountById(giro);
        QCOMPARE(account.getName(), QString("Hausbank"));
        QCOMPARE(account.getType(), QString("Savings"));
        QCOMPARE(account.getBalanceCents(), Cents(5000));
    }

    void refusesAnEmptyNameAndUnknownThings() {
        QVERIFY(!AccountDAO().editAccount(giro, "   ", typeId(1, "Checking")));
        QVERIFY(!AccountDAO().editAccount(giro, "Hausbank", 999999));
        QVERIFY(!AccountDAO().editAccount(999999, "Hausbank", typeId(1, "Checking")));
        QCOMPARE(AccountDAO().getAccountById(giro).getName(), QString("Girokonto"));
    }

    void refusesAnotherUsersType() {
        QVERIFY(UserDAO().createUser(User(0, "other", "Other User")));
        const int otherType = AccountDAO().addAccountType(2, "Foreign");
        QVERIFY(otherType > 0);
        QVERIFY(!AccountDAO().editAccount(giro, "Hausbank", otherType));
    }

    void takenNamesIgnoreCaseAndTheAccountItself() {
        AccountDAO dao;
        QVERIFY(dao.isNameTaken(1, " tagesgeld "));
        QVERIFY(dao.isNameTaken(1, "GIROKONTO"));
        QVERIFY(!dao.isNameTaken(1, "GIROKONTO", giro));
        QVERIFY(!dao.isNameTaken(1, "Hausbank"));
    }

    void dialogStartsWithTheAccountAndSaves() {
        EditAccountDialog dialog(1, giro);
        auto* name = dialog.findChild<QLineEdit*>("accountNameEdit");
        auto* type = dialog.findChild<QComboBox*>("accountTypeCombo");
        QCOMPARE(name->text(), QString("Girokonto"));
        QCOMPARE(type->currentText(), QString("Checking"));

        name->setText("Hausbank");
        type->setCurrentIndex(type->findText("Investment"));
        dialog.findChild<QPushButton*>("saveAccountButton")->click();

        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        const Account account = AccountDAO().getAccountById(giro);
        QCOMPARE(account.getName(), QString("Hausbank"));
        QCOMPARE(account.getType(), QString("Investment"));
    }

    void dialogWontSaveAnEmptyOrTakenName() {
        EditAccountDialog dialog(1, giro);
        auto* name = dialog.findChild<QLineEdit*>("accountNameEdit");
        auto* save = dialog.findChild<QPushButton*>("saveAccountButton");
        auto* status = dialog.findChild<QLabel*>("accountStatusLabel");
        QVERIFY(save->isEnabled());

        name->setText("  ");
        QVERIFY(!save->isEnabled());
        name->setText("tagesgeld");
        QVERIFY(!save->isEnabled());
        QVERIFY(!status->text().isEmpty());
        name->setText("girokonto");                               // Only its own name in another case
        QVERIFY(save->isEnabled());
        QVERIFY(status->text().isEmpty());
    }
};

QTEST_MAIN(TestEditAccount)
#include "test_editaccount.moc"
