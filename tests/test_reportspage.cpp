#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>

#include "account.h"
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
};

QTEST_MAIN(TestReportsPage)
#include "test_reportspage.moc"
