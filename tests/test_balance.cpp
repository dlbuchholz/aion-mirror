#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

#include "account.h"
#include "balance.h"
#include "budget.h"
#include "dashboard.h"
#include "dialog/balancecorrectiondialog.h"
#include "dialog/importmanagerdialog.h"
#include "settings.h"
#include "tests/test_db_fixture.h"

class TestBalance : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;
    int savings = -1;

    void add(int account, Cents cents, const QDate& date, const char* type) {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, account, "Shop", cents, date, "", 0, type)) > 0);
    }

    static QString categoryOf(int transactionId) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT c.name FROM transactions t JOIN categories c ON c.id = t.category_id WHERE t.id = :id");
        query.bindValue(":id", transactionId);
        return query.exec() && query.next() ? query.value(0).toString() : QString();
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        savings = AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0);
    }
    void cleanup() { db.close(); }

    void balanceOnADayIncludesThatDay() {
        add(giro, 10000, QDate(2026, 10, 1), "income");
        add(giro, 2500, QDate(2026, 10, 17), "expense");
        add(giro, 999, QDate(2026, 10, 18), "expense");
        add(savings, 50000, QDate(2026, 10, 1), "income");
        QCOMPARE(Balance::onDate(giro, QDate(2026, 9, 30)), Cents(0));
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 17)), Cents(7500));
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 18)), Cents(6501));
    }

    void correctionsGoEitherWay() {
        const int down = Balance::addCorrection(giro, QDate(2026, 10, 17), -1240, "Gap in the export");
        QVERIFY(down > 0);
        QCOMPARE(categoryOf(down), QString("Balance correction"));
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 17)), Cents(-1240));
        const int up = Balance::addCorrection(giro, QDate(2026, 10, 18), 5000, "");
        QVERIFY(up > 0);
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 18)), Cents(3760));
        QCOMPARE(Balance::addCorrection(giro, QDate(2026, 10, 18), 0, ""), 0);   // Nothing to correct
    }

    void correctionsArentIncomeOrSpending() {
        const QDate today(2026, 10, 18);
        QVERIFY(BudgetDAO().setMonthlyLimit(1, 4, today, 10000));                // Any budget, to read spending
        Balance::addCorrection(giro, today, -3000, "");
        Balance::addCorrection(giro, today, 2000, "");
        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.thisMonth.incomeCents, Cents(0));
        QCOMPARE(data.thisMonth.expenseCents, Cents(0));
        QVERIFY(data.spendingByCategory.isEmpty());
        QCOMPARE(data.totalBalanceCents, Cents(-1000));                           // But the balance changes
        QVERIFY(BudgetDAO().unbudgetedSpending(1, today).isEmpty());
    }

    void dialogAddsTheDifference() {
        add(giro, 10000, QDate::currentDate(), "income");
        BalanceCorrectionDialog dialog(1, giro);
        auto* add = dialog.findChild<QPushButton*>("addCorrectionButton");
        QCOMPARE(dialog.findChild<QLabel*>("currentBalanceLabel")->text(), Settings::instance().formatCurrency(10000));
        QVERIFY(!add->isEnabled());                                                // Nothing entered yet

        auto* bank = dialog.findChild<QLineEdit*>("bankBalanceEdit");
        bank->setText(Settings::instance().formatCurrency(10000));
        QVERIFY(!add->isEnabled());                                                // Already matches
        bank->setText(Settings::instance().formatCurrency(8760));
        QVERIFY(add->isEnabled());
        add->click();

        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(dialog.addedTransactionId() > 0);
        QCOMPARE(categoryOf(dialog.addedTransactionId()), QString("Balance correction"));
        QCOMPARE(Balance::onDate(giro, QDate::currentDate()), Cents(8760));
    }

    void importDialogComparesWithTheBanksBalance() {
        add(giro, 120410, QDate(2026, 9, 30), "income");
        QTemporaryDir dir;
        const QString path = dir.filePath("umsaetze.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Buchungstag;Betrag;Name Zahlungsbeteiligter;Verwendungszweck;Saldo nach Buchung\n"
                   "17.10.2026;-31,20;Rewe;Wocheneinkauf;1.510,25\n"
                   "03.10.2026;350,00;ACME;Gehalt;1.541,45\n");
        file.close();

        ImportManagerDialog dialog(1);
        dialog.findChild<QComboBox*>("accountComboBox")->setCurrentIndex(0);
        dialog.loadFile(path);
        auto* status = dialog.findChild<QLabel*>("balanceStatusLabel");
        auto* correction = dialog.findChild<QCheckBox*>("correctionCheckBox");
        QTRY_VERIFY(dialog.findChild<QLabel*>("balanceAfterValue")->text() != "–");

        // 1.204,10 + 318,80 = 1.522,90 after the import; the file's balance says 1.510,25
        QCOMPARE(dialog.findChild<QLabel*>("balanceAfterValue")->text(), Settings::instance().formatCurrency(152290));
        QCOMPARE(dialog.findChild<QLineEdit*>("expectedBalanceEdit")->text(), Settings::instance().formatCurrency(151025));
        QVERIFY(correction->isVisibleTo(&dialog));
        QVERIFY(correction->text().contains(Settings::instance().formatCurrency(-1265, true)));
        QVERIFY(correction->isChecked());

        // Typing the matching balance hides the correction
        auto* expected = dialog.findChild<QLineEdit*>("expectedBalanceEdit");
        expected->setText(Settings::instance().formatCurrency(152290));
        emit expected->textEdited(expected->text());
        QVERIFY(!correction->isVisibleTo(&dialog));
        QVERIFY(status->text().startsWith("Matches"));
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 17)), Cents(120410));    // Still nothing written
    }

    void importButtonsStayOutsideTheScrollingPart() {
        ImportManagerDialog dialog(1);
        auto* scroll = dialog.findChild<QWidget*>("importScroll");
        QVERIFY(scroll);
        QVERIFY(scroll->isAncestorOf(dialog.findChild<QWidget*>("balanceGroupBox")));
        QVERIFY(!scroll->isAncestorOf(dialog.findChild<QPushButton*>("importButton")));
        QVERIFY(!scroll->isAncestorOf(dialog.findChild<QPushButton*>("dryRunButton")));
    }

    void manyFileCategoriesStayCompact() {
        QTemporaryDir dir;
        const QString path = dir.filePath("many.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Buchungstag;Betrag;Name;Kategorie\n10.10.2026;-1,00;A;Groceries\n");
        for (int i = 1; i <= 30; ++i) file.write(QString("10.10.2026;-%1,00;Shop;Neu %1\n").arg(i).toUtf8());
        file.close();

        ImportManagerDialog dialog(1);
        dialog.loadFile(path);
        auto* summary = dialog.findChild<QLabel*>("categorySummaryLabel");
        QCOMPARE(summary->text(), QString("31 categories in the file: 1 already in Aion, 30 will be created."));
        QVERIFY(dialog.findChild<QWidget*>("categoriesScroll")->height() <= 280);   // The list scrolls on its own

        // One click for all new ones
        auto* button = dialog.findChild<QToolButton*>("newCategoriesButton");
        button->menu()->actions().at(1)->trigger();
        QCOMPARE(summary->text(), QString("31 categories in the file: 1 already in Aion, 30 imported without a category."));
        for (QComboBox* choice : dialog.findChildren<QComboBox*>("categoryChoice")) {
            QCOMPARE(choice->currentData().toString(), QString("none"));
        }
    }

    void dialogChecksAnEarlierDay() {
        add(giro, 10000, QDate(2026, 1, 10), "income");
        add(giro, 3000, QDate(2026, 2, 10), "expense");
        BalanceCorrectionDialog dialog(1, giro);
        dialog.findChild<QDateEdit*>()->setDate(QDate(2026, 1, 31));
        QCOMPARE(dialog.findChild<QLabel*>("currentBalanceLabel")->text(), Settings::instance().formatCurrency(10000));
        dialog.findChild<QLineEdit*>("bankBalanceEdit")->setText("not money");
        QVERIFY(!dialog.findChild<QPushButton*>("addCorrectionButton")->isEnabled());
    }
};

QTEST_MAIN(TestBalance)
#include "test_balance.moc"
