#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

#include "account.h"
#include "balance.h"
#include "budget.h"
#include "category.h"
#include "csvimport.h"
#include "dashboard.h"
#include "dialog/balancecorrectiondialog.h"
#include "dialog/importmanagerdialog.h"
#include "rule.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "tests/test_spreadsheet_files.h"

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
        QVERIFY(data.recent.isEmpty());                                           // And they aren't "recent transactions"
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
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();                                                           // Columns
        next->click();                                                           // No category column: on to the review
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        auto* status = dialog.findChild<QLabel*>("balanceStatusLabel");
        auto* correction = dialog.findChild<QCheckBox*>("correctionCheckBox");
        QCOMPARE(dialog.findChild<QLabel*>("newCountLabel")->text(), QString("2"));
        QCOMPARE(dialog.findChild<QPushButton*>("importButton")->text(), QString("Import 2 transactions"));

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

    void importUsesTheRulesWhenTicked() {
        int groceries = -1;
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == "Groceries") groceries = category.getCategoryId();
        }
        Rule rule;
        rule.categoryId = groceries;
        rule.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::Contains, "rewe")};
        QVERIFY(RuleDAO(1).addRule(rule) > 0);
        QTemporaryDir dir;
        const QString path = dir.filePath("umsaetze.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Buchungstag;Betrag;Name Zahlungsbeteiligter;Verwendungszweck\n"
                   "17.10.2026;-31,20;Rewe;Wocheneinkauf\n"
                   "03.10.2026;350,00;ACME;Gehalt\n");
        file.close();

        ImportManagerDialog dialog(1);
        dialog.findChild<QComboBox*>("accountComboBox")->setCurrentIndex(0);
        dialog.loadFile(path);
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();
        next->click();                                                           // No category column: review
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        auto* useRules = dialog.findChild<QCheckBox*>("useRulesCheck");
        auto* rulesLabel = dialog.findChild<QLabel*>("rulesLabel");
        QVERIFY(useRules->isChecked());
        QVERIFY(!dialog.findChild<QCheckBox*>("rulesOverrideCheck")->isVisibleTo(&dialog));   // Nothing to override
        QCOMPARE(rulesLabel->text(), QString("1 new transaction gets its category from your rules."));
        useRules->setChecked(false);
        QVERIFY(!rulesLabel->isVisibleTo(&dialog));
        useRules->setChecked(true);
        QVERIFY(rulesLabel->isVisibleTo(&dialog));
        // Importing itself shows a message box; test_csvimport checks the categories it writes
    }

    void importReadsTheTickedSheetsOfASpreadsheet() {
        const QStringList header = {"Buchungstag", "Betrag", "Name Zahlungsbeteiligter"};
        QTemporaryDir dir;
        const QString path = dir.filePath("umsaetze.xlsx");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(xlsxFile({
            {"Januar", {header, {"17.01.2026", "-31.2", "Rewe"}, {"03.01.2026", "350", "ACME"}}},
            {"Februar", {header, {"02.02.2026", "-9.99", "Netflix"}}},
            {"Leer", {}},
            {"Notizen", {{"Was", "Wann"}, {"Steuer", "März"}}},
        }));
        file.close();

        ImportManagerDialog dialog(1);
        dialog.findChild<QComboBox*>("accountComboBox")->setCurrentIndex(0);
        dialog.loadFile(path);
        auto* summary = dialog.findChild<QLabel*>("formatSummaryLabel");
        QCOMPARE(summary->text(), QString("Excel · 2 of 4 sheets · 3 rows"));   // Februar's header row is skipped

        // Same columns ticked, different ones and empty sheets not
        const QList<QCheckBox*> checks = dialog.findChildren<QCheckBox*>("sheetCheck");
        QCOMPARE(checks.size(), 4);
        QVERIFY(checks[0]->isChecked() && checks[1]->isChecked());
        QVERIFY(!checks[2]->isChecked() && !checks[2]->isEnabled());
        QVERIFY(!checks[3]->isChecked());
        QStringList statuses;
        for (QLabel* status : dialog.findChildren<QLabel*>("sheetStatus")) statuses << status->text();
        QCOMPARE(statuses, (QStringList{"Buchungstag · Betrag · Name Zahlungsbeteiligter", "same columns", "empty",
                                        "different columns"}));

        // Without Januar, Februar names the columns
        checks[0]->setChecked(false);
        QCOMPARE(summary->text(), QString("Excel · 1 of 4 sheets · 1 row"));
        QCOMPARE(dialog.findChildren<QLabel*>("sheetStatus").at(1)->text(), QString("Buchungstag · Betrag · Name Zahlungsbeteiligter"));
        checks[0]->setChecked(true);

        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();                                                           // Columns, guessed from the header
        QCOMPARE(dialog.findChild<QComboBox*>("map_Amount")->currentText(), QString("Betrag"));
        next->click();                                                           // No category column: review
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        QCOMPARE(dialog.findChild<QLabel*>("newCountLabel")->text(), QString("3"));
        QCOMPARE(dialog.findChild<QLabel*>("failedCountLabel")->text(), QString("0"));
        QCOMPARE(dialog.findChild<QLabel*>("balanceAfterValue")->text(), Settings::instance().formatCurrency(30881));   // 350 − 31.20 − 9.99

        // A sheet with other columns ticked anyway: its rows can't be read, and say where they are
        auto* back = dialog.findChild<QPushButton*>("backButton");
        back->click();
        back->click();
        checks[3]->setChecked(true);
        next->click();
        next->click();
        QCOMPARE(dialog.findChild<QLabel*>("failedCountLabel")->text(), QString("2"));
        QVERIFY2(dialog.findChild<QLabel*>("problemsLabel")->text().contains("Notizen, row 1: Invalid date 'Was'"),
                 qPrintable(dialog.findChild<QLabel*>("problemsLabel")->text()));
    }

    void sheetsWithSlightlyOtherHeadersAreMatchedByName() {
        // Like a real bank's yearly workbook: one month without the "Monat" heading, one with a
        // renamed column the import doesn't use, one in another order
        const QStringList header = {"Monat", "Buchungstag", "Name Zahlungsbeteiligter", "Betrag", "Saldo nach Buchung", "Steuerrelevant"};
        QTemporaryDir dir;
        const QString path = dir.filePath("jahr.xlsx");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(xlsxFile({
            {"Januar", {header, {"Jan", "17.01.2026", "Rewe", "-31.2", "68.8", "nein"}, {"Jan", "03.01.2026", "ACME", "100", "100", "nein"}}},
            {"Februar", {{"", "Buchungstag", "Name Zahlungsbeteiligter", "Betrag", "Saldo nach Buchung", "Steuerrelevant"},
                         {"Feb", "02.02.2026", "Netflix", "-9.99", "58.81", "nein"}}},
            {"März", {{"Monat", "Buchungstag", "Name Zahlungsbeteiligter", "Betrag", "Saldo nach Buchung", "Gekennzeichneter Umsatz"},
                      {"Mär", "05.03.2026", "Stadtwerke", "-50", "8.81", ""}}},
            {"April", {{"Betrag", "Buchungstag", "Name Zahlungsbeteiligter", "Saldo nach Buchung"},
                       {"-8.81", "01.04.2026", "Bäcker", "0"}}},
        }));
        file.close();

        ImportManagerDialog dialog(1);
        dialog.findChild<QComboBox*>("accountComboBox")->setCurrentIndex(0);
        dialog.loadFile(path);
        QCOMPARE(dialog.findChild<QLabel*>("formatSummaryLabel")->text(), QString("Excel · 4 of 4 sheets · 5 rows"));
        QStringList statuses;
        for (QLabel* status : dialog.findChildren<QLabel*>("sheetStatus")) statuses << status->text();
        QCOMPARE(statuses, (QStringList{header.join(" · "), "same columns", "without Steuerrelevant",
                                        "without Monat, Steuerrelevant"}));

        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        QCOMPARE(dialog.findChild<QLabel*>("newCountLabel")->text(), QString("5"));
        QCOMPARE(dialog.findChild<QLabel*>("failedCountLabel")->text(), QString("0"));
        // The bank's balance after the last booking matches: nothing to correct
        QCOMPARE(dialog.findChild<QLabel*>("balanceAfterValue")->text(), Settings::instance().formatCurrency(0));
        QVERIFY(!dialog.findChild<QCheckBox*>("correctionCheckBox")->isVisibleTo(&dialog));
    }

    void aSpreadsheetWithOneSheetHasNothingToChoose() {
        QTemporaryDir dir;
        const QString path = dir.filePath("konto.xlsx");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(xlsxFile({{"Konto", {{"Datum", "Betrag"}, {"2026-10-01", "12.5"}, {"2026-10-02", "-3"}}}}));
        file.close();

        ImportManagerDialog dialog(1);
        dialog.loadFile(path);
        QCOMPARE(dialog.findChild<QLabel*>("formatSummaryLabel")->text(), QString("Excel · 2 rows"));
        QVERIFY(!dialog.findChild<QFrame*>("sheetsBox")->isVisibleTo(&dialog));
    }

    void aDamagedSpreadsheetKeepsTheFileBefore() {
        QTemporaryDir dir;
        const QString csv = dir.filePath("umsaetze.csv");
        QFile csvFile(csv);
        QVERIFY(csvFile.open(QIODevice::WriteOnly | QIODevice::Text));
        csvFile.write("Datum;Betrag\n01.10.2026;-3,00\n");
        csvFile.close();
        const QString broken = dir.filePath("kaputt.xlsx");
        QFile brokenFile(broken);
        QVERIFY(brokenFile.open(QIODevice::WriteOnly));
        brokenFile.write("not a zip");
        brokenFile.close();

        ImportManagerDialog dialog(1);
        dialog.loadFile(csv);
        dialog.loadFile(broken);
        QVERIFY(dialog.findChild<QLineEdit*>("filePathEdit")->text().endsWith("umsaetze.csv"));
        QCOMPARE(dialog.findChild<QLabel*>("formatSummaryLabel")->text(), QString("1 row · semicolon separated · decimal comma"));
    }

    void importWizardChecksEachStep() {
        QTemporaryDir dir;
        const QString path = dir.filePath("plain.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("When,How much,Who\n2026-10-17,-12.50,Bakery\n2026-10-18,oops,Kiosk\n");
        file.close();

        ImportManagerDialog dialog(1);
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        auto* back = dialog.findChild<QPushButton*>("backButton");
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::File);        // No file yet

        dialog.loadFile(path);
        QCOMPARE(dialog.findChild<QLabel*>("formatSummaryLabel")->text(), QString("2 rows · comma separated · decimal point"));
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Columns);
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Columns);     // Headers unknown: date and amount needed

        auto* date = dialog.findChild<QComboBox*>("map_Date");
        auto* amount = dialog.findChild<QComboBox*>("map_Amount");
        date->setCurrentIndex(date->findData(0));
        amount->setCurrentIndex(amount->findData(1));
        dialog.findChild<QComboBox*>("map_Payee")->setCurrentIndex(3);
        QVERIFY(dialog.findChild<QLabel*>("example_Date")->text().contains("17 October 2026"));
        QVERIFY(dialog.findChild<QLabel*>("example_Amount")->text().contains("expense"));

        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        QCOMPARE(dialog.findChild<QLabel*>("failedCountLabel")->text(), QString("1"));
        QVERIFY(dialog.findChild<QLabel*>("problemsLabel")->text().contains("Skip rows that can't be read"));
        QVERIFY(!dialog.findChild<QPushButton*>("importButton")->isEnabled());  // Nothing would be imported

        back->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Columns);     // Categories skipped both ways
        dialog.findChild<QCheckBox*>("allowPartialCheck")->setChecked(true);
        next->click();
        QCOMPARE(dialog.findChild<QPushButton*>("importButton")->text(), QString("Import 1 transaction"));
        QVERIFY(dialog.findChild<QPushButton*>("importButton")->isEnabled());
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
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Categories);
        auto* summary = dialog.findChild<QLabel*>("categorySummaryLabel");
        QCOMPARE(summary->text(), QString("31 categories in the file: 1 already in Aion, 30 will be created."));

        // One click for all new ones
        auto* button = dialog.findChild<QToolButton*>("newCategoriesButton");
        button->menu()->actions().at(1)->trigger();
        QCOMPARE(summary->text(), QString("31 categories in the file: 1 already in Aion, 30 imported without a category."));
        for (QComboBox* choice : dialog.findChildren<QComboBox*>("categoryChoice")) {
            QCOMPARE(choice->currentData().toString(), QString("none"));
        }
        next->click();
        QVERIFY(dialog.findChild<QLabel*>("newCategoriesLabel")->isHidden());   // None are created now
        dialog.findChild<QPushButton*>("backButton")->click();
        button->menu()->actions().at(0)->trigger();
        next->click();
        QCOMPARE(dialog.findChild<QLabel*>("newCategoriesLabel")->text(), QString("New categories: Neu 1, Neu 10 and 28 more"));
    }

    void wizardCanFinishWhenEverythingIsAlreadyImported() {
        QTemporaryDir dir;
        const QString path = dir.filePath("again.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Buchungstag;Betrag;Name;Saldo nach Buchung\n17.10.2026;-31,20;Rewe;968,80\n03.10.2026;1000,00;ACME;1.000,00\n");
        file.close();

        // The same file imported before, matching the bank
        CsvImport::Options options;
        options.accountId = giro;
        options.decimalSeparator = ',';
        const CsvImport::Mapping mapping = {{CsvImport::FieldDate, 0}, {CsvImport::FieldAmount, 1}, {CsvImport::FieldPayee, 2}};
        QCOMPARE(CsvImport::run({{"17.10.2026", "-31,20", "Rewe"}, {"03.10.2026", "1000,00", "ACME"}}, mapping, options, {}).imported, 2);

        ImportManagerDialog dialog(1);
        dialog.loadFile(path);
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();
        next->click();
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        QCOMPARE(dialog.findChild<QLabel*>("newCountLabel")->text(), QString("0"));
        QCOMPARE(dialog.findChild<QLabel*>("duplicateCountLabel")->text(), QString("2"));
        QVERIFY(dialog.findChild<QLabel*>("balanceStatusLabel")->text().startsWith("Matches"));
        auto* finish = dialog.findChild<QPushButton*>("importButton");
        QCOMPARE(finish->text(), QString("Close"));
        QVERIFY(finish->isEnabled());

        // A difference from before: explained as such, and closing still works without a correction
        Balance::addCorrection(giro, QDate(2026, 10, 1), 500, "");
        dialog.findChild<QPushButton*>("backButton")->click();
        next->click();
        QVERIFY(dialog.findChild<QLabel*>("balanceStatusLabel")->text().contains("This file adds nothing"));
        QCOMPARE(finish->text(), QString("Add balance correction"));
        dialog.findChild<QCheckBox*>("correctionCheckBox")->setChecked(false);
        QCOMPARE(finish->text(), QString("Close"));
        finish->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(Balance::onDate(giro, QDate(2026, 10, 17)), Cents(97380));     // Nothing was added
    }

    void importerCanCreateTheAccount() {
        ImportManagerDialog dialog(1);
        QSignalSpy created(&dialog, &ImportManagerDialog::accountCreated);
        auto* accounts = dialog.findChild<QComboBox*>("accountComboBox");
        QCOMPARE(accounts->currentData().toInt(), giro);                       // An existing account comes first
        QVERIFY(dialog.findChild<QFrame*>("newAccountBox")->isHidden());

        accounts->setCurrentIndex(accounts->findData(ImportManagerDialog::NewAccount));
        QVERIFY(!dialog.findChild<QFrame*>("newAccountBox")->isHidden());
        auto* name = dialog.findChild<QLineEdit*>("newAccountNameEdit");
        auto* create = dialog.findChild<QPushButton*>("createAccountButton");
        name->setText("girokonto");
        create->click();
        QCOMPARE(created.count(), 0);                                           // Already exists

        name->setText("Kreditkarte");
        dialog.findChild<QComboBox*>("newAccountTypeCombo")->setCurrentText("Checking");
        create->click();
        QCOMPARE(created.count(), 1);
        const int newId = created[0][0].toInt();
        QCOMPARE(accounts->currentData().toInt(), newId);                      // Selected right away
        QCOMPARE(accounts->currentText(), QString("Kreditkarte"));
        QVERIFY(dialog.findChild<QFrame*>("newAccountBox")->isHidden());
        QCOMPARE(Balance::onDate(newId, QDate::currentDate()), Cents(0));       // No starting balance
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
