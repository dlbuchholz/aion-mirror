#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QToolButton>

#include "account.h"
#include "transaction.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/reportspage.h"

using Preset = ReportRange::Preset;

class TestReportsPage : public QObject {
    Q_OBJECT
    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;

    static QString text(ReportsPage& page, const char* name) { return page.findChild<QLabel*>(name)->text(); }
    static QToolButton* chip(ReportsPage& page, Preset preset) {
        return page.findChild<QToolButton*>("reportsRange_" + ReportRange::key(preset));
    }
    static QList<QAbstractButton*> cards(ReportsPage& page) {
        return page.findChildren<QAbstractButton*>("reportCard");
    }

private slots:
    void initTestCase() {
        db.init();
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }
    void cleanupTestCase() {
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        QSettings& settings = Settings::instance().getQSettings();
        for (const char* key : {"ui/reportsRange", "ui/reportsFrom", "ui/reportsTo"}) settings.remove(key);
    }
    void cleanup() { db.close(); }

    void startsWithSixMonthsAndRemembersTheChoice() {
        const QDate today = QDate::currentDate();
        {
            ReportsPage page;
            page.setUserId(1);
            QVERIFY(chip(page, Preset::SixMonths)->isChecked());
            QCOMPARE(page.range(), ReportRange::forPreset(Preset::SixMonths, today));
            QCOMPARE(text(page, "reportsRangeLabel"), page.range().label());

            chip(page, Preset::Year)->click();
            QCOMPARE(page.range(), ReportRange::forPreset(Preset::Year, today));
            QCOMPARE(text(page, "reportsRangeLabel"), page.range().label());
        }
        ReportsPage again;
        again.setUserId(1);
        QVERIFY(chip(again, Preset::Year)->isChecked());
        QCOMPARE(again.range().months(), 12);
    }

    void customRangesAreAskedForAndRemembered() {
        {
            ReportsPage page;
            page.setUserId(1);
            page.askCustomRange = [](QDate& from, QDate& to) {
                from = QDate(2026, 10, 15);
                to = QDate(2026, 9, 1);                  // Back to front is fine
                return true;
            };
            chip(page, Preset::Custom)->click();
            QVERIFY(chip(page, Preset::Custom)->isChecked());
            QCOMPARE(page.range(), ReportRange::custom(QDate(2026, 9, 1), QDate(2026, 10, 15)));
            QCOMPARE(text(page, "reportsRangeLabel"), QString("1 Sep – 15 Oct 2026"));

            // Cancelling keeps what was there
            chip(page, Preset::Month)->click();
            QDate askedFrom;
            page.askCustomRange = [&askedFrom](QDate& from, QDate&) {
                askedFrom = from;
                return false;
            };
            chip(page, Preset::Custom)->click();
            QCOMPARE(askedFrom, page.range().first);     // Starts from the shown range
            QVERIFY(chip(page, Preset::Month)->isChecked());
            QCOMPARE(page.range().months(), 1);

            page.askCustomRange = [](QDate& from, QDate& to) {
                from = QDate(2026, 3, 1);
                to = QDate(2026, 3, 20);
                return true;
            };
            chip(page, Preset::Custom)->click();
        }
        ReportsPage again;
        again.setUserId(1);
        QCOMPARE(again.range(), ReportRange::custom(QDate(2026, 3, 1), QDate(2026, 3, 20)));
    }

    void cardsOpenTheirReport() {
        ReportsPage page;
        page.setUserId(1);
        const QList<QAbstractButton*> all = cards(page);
        QCOMPARE(all.size(), 6);
        QCOMPARE(all.first()->accessibleName(), QString("Spending by category"));
        auto* back = page.findChild<QToolButton*>("reportsBackLink");
        QVERIFY(back->isHidden());

        all[2]->click();
        QCOMPARE(page.openedReport(), ReportsPage::Report::Balance);
        QCOMPARE(text(page, "reportsTitle"), QString("Balance over time"));
        QVERIFY(!back->isHidden());
        QCOMPARE(page.findChild<QStackedWidget*>()->currentWidget()->objectName(), QString("reportsDetail"));

        // The range stays when going back and forth
        chip(page, Preset::ThreeMonths)->click();
        back->click();
        QVERIFY(!page.openedReport());
        QCOMPARE(text(page, "reportsTitle"), QString("Reports"));
        QVERIFY(back->isHidden());
        QVERIFY(chip(page, Preset::ThreeMonths)->isChecked());
    }

    void accountsCanBeChosen() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0);
        ReportsPage page;
        page.setUserId(1);
        auto* combo = page.findChild<QComboBox*>("reportsAccountCombo");
        QCOMPARE(combo->count(), 3);
        QCOMPARE(combo->itemText(0), QString("All accounts"));
        QCOMPARE(page.accountId(), ReportsPage::ALL_ACCOUNTS);

        combo->setCurrentIndex(combo->findData(giro));
        emit combo->activated(combo->currentIndex());
        QCOMPARE(page.accountId(), giro);
        page.reload();                                   // Still there after a reload
        QCOMPARE(page.accountId(), giro);
        QCOMPARE(combo->currentText(), QString("Girokonto"));

        QVERIFY(AccountDAO().deleteAccount(giro));
        page.reload();                                   // Gone: back to all of them
        QCOMPARE(page.accountId(), ReportsPage::ALL_ACCOUNTS);
        QCOMPARE(combo->count(), 2);
    }

    void spendingByCategoryOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const int savings = AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 3000, today, "", groceries, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 1000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        QAbstractButton* card = cards(page).first();
        QVERIFY(card->accessibleDescription().startsWith(Settings::instance().formatSpending(4000)));

        card->click();
        QVERIFY(text(page, "reportsRangeLabel").contains(" · vs. the month before"));
        QList<QWidget*> rows = page.findChildren<QWidget*>("spendingRow");
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0]->accessibleName(), QString("Groceries"));
        QVERIFY(rows[0]->accessibleDescription().contains("75% of all spending"));
        QCOMPARE(rows[1]->accessibleName(), QString("No category"));
        QVERIFY(page.findChild<QWidget*>("spendingTotal"));

        // Nothing spent from the savings account
        auto* combo = page.findChild<QComboBox*>("reportsAccountCombo");
        combo->setCurrentIndex(combo->findData(savings));
        emit combo->activated(combo->currentIndex());
        QVERIFY(page.findChildren<QWidget*>("spendingRow").isEmpty());
        QVERIFY(page.findChild<QLabel*>("reportsEmptyLabel"));
        QCOMPARE(card->accessibleDescription(), QString("No spending in this range"));
    }

    void incomeVsSpendingOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Employer", 200000, today, "", 0, "income"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 50000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::ThreeMonths)->click();
        QAbstractButton* card = cards(page)[1];
        QVERIFY(card->accessibleDescription().startsWith("Kept 75%"));

        card->click();
        QCOMPARE(page.openedReport(), ReportsPage::Report::IncomeVsSpending);
        QVERIFY(page.findChild<QWidget*>("flowChart"));
        const QList<QWidget*> rows = page.findChildren<QWidget*>("flowRow");
        QCOMPARE(rows.size(), 3);                        // A row per month
        QCOMPARE(rows.last()->accessibleName(), ReportRange::forPreset(Preset::Month, today).label());
        QVERIFY(page.findChild<QWidget*>("flowTotal"));

        // More spent than came in
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 300000, today, "", 0, "expense"));
        page.reload();
        QVERIFY(card->accessibleDescription().startsWith("Spent more, "));
    }

    void balanceOverTimeOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Start", 100000, today.addMonths(-4), "", 0, "income"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 25000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        QAbstractButton* card = cards(page)[2];
        const Settings& settings = Settings::instance();
        QCOMPARE(card->accessibleDescription(),
                 QString("%1, %2 since %3").arg(settings.formatCurrency(75000), settings.formatCurrency(-25000, true),
                                                QLocale(QLocale::English).toString(QDate(today.year(), today.month(), 1), "d MMMM")));

        card->click();
        QVERIFY(page.findChild<QWidget*>("balanceChart"));
        int weeksSoFar = 0;                              // Weeks still to come aren't listed
        for (const ReportRange& week : ReportRange::forPreset(Preset::Month, today).periods(ReportRange::Unit::Week)) {
            if (week.first <= today) ++weeksSoFar;
        }
        QCOMPARE(page.findChildren<QWidget*>("balanceRow").size(), weeksSoFar);
        QCOMPARE(page.findChild<QWidget*>("balanceTotal")->accessibleName(), QString("Whole range"));
    }

    void categoryTrendFollowsTheLargestOrTheChosenCategory() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 3000, today, "", groceries, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 1000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::ThreeMonths)->click();
        QAbstractButton* card = cards(page)[3];
        QVERIFY(card->accessibleDescription().startsWith("Groceries, "));   // The largest
        QCOMPARE(page.trendCategory(), groceries);

        // From the spending list: the clicked category
        cards(page)[0]->click();
        QList<QAbstractButton*> rows = page.findChildren<QAbstractButton*>("spendingRow");
        QCOMPARE(rows[1]->accessibleName(), QString("No category"));
        rows[1]->click();
        QCOMPARE(page.openedReport(), ReportsPage::Report::CategoryTrend);
        QCOMPARE(page.trendCategory(), 0);
        QVERIFY(card->accessibleDescription().startsWith("No category, "));
        QCOMPARE(page.findChildren<QWidget*>("trendRow").size(), 3);       // A row per month
        QVERIFY(page.findChild<QWidget*>("trendChart"));

        // The picker changes it
        auto* picker = page.findChild<QComboBox*>("trendCategoryCombo");
        QCOMPARE(picker->currentData().toInt(), 0);
        picker->setCurrentIndex(picker->findData(groceries));
        emit picker->activated(picker->currentIndex());
        QCOMPARE(page.trendCategory(), groceries);
        QCOMPARE(page.findChild<QComboBox*>("trendCategoryCombo")->currentData().toInt(), groceries);

        // On to the transactions: the whole range, or one month
        QSignalSpy requested(&page, &ReportsPage::transactionsRequested);
        page.findChild<QPushButton*>("trendTransactionsButton")->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested[0][0].toInt(), groceries);
        QCOMPARE(requested[0][1].toInt(), ReportsPage::ALL_ACCOUNTS);
        QCOMPARE(requested[0][2].toDate(), page.range().first);
        QCOMPARE(requested[0][3].toDate(), page.range().last);
        page.findChildren<QAbstractButton*>("trendRow").last()->click();
        QCOMPARE(requested.size(), 2);
        QCOMPARE(requested[1][2].toDate(), QDate(today.year(), today.month(), 1));
        QCOMPARE(requested[1][3].toDate(), page.range().last);
    }
};

QTEST_MAIN(TestReportsPage)
#include "test_reportspage.moc"
