#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

#include "account.h"
#include "budget.h"
#include "transaction.h"
#include "settings.h"
#include "theme.h"
#include "tests/test_db_fixture.h"
#include "widgets/budgetcharts.h"
#include "widgets/periodpicker.h"
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
    // Clicking the period's name asks for the first and last day
    static void chooseDays(ReportsPage& page) { page.findChild<QToolButton*>("periodName")->click(); }
    static QString periodName(ReportsPage& page) { return page.findChild<QToolButton*>("periodName")->text(); }
    static bool noLengthChecked(ReportsPage& page) {
        for (Preset length : {Preset::Month, Preset::ThreeMonths, Preset::SixMonths, Preset::Year}) {
            if (chip(page, length)->isChecked()) return false;
        }
        return true;
    }
    static QToolButton* viewChip(ReportsPage& page, const QString& view) {
        return page.findChild<QToolButton*>("budgetView_" + view);
    }
    static QWidget* shownView(ReportsPage& page) {
        const QList<QWidget*> views = page.findChild<QWidget*>("budgetViews")->findChildren<QWidget*>(Qt::FindDirectChildrenOnly);
        QWidget* shown = nullptr;
        for (QWidget* view : views) {
            if (view->isHidden()) continue;
            if (shown) return nullptr;                   // Only ever one
            shown = view;
        }
        return shown;
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
        for (const char* key : {"ui/reportsRange", "ui/reportsFrom", "ui/reportsTo", "appearance/reportsCentred"}) settings.remove(key);
    }
    void cleanup() { db.close(); }

    void startsWithSixMonthsAndRemembersTheChoice() {
        const QDate today = QDate::currentDate();
        {
            ReportsPage page;
            page.setUserId(1);
            QVERIFY(chip(page, Preset::SixMonths)->isChecked());
            QCOMPARE(page.range(), ReportRange::forPreset(Preset::SixMonths, today));
            QCOMPARE(text(page, "reportsRangeLabel"), QString("6 months, to today"));

            chip(page, Preset::Year)->click();
            QCOMPARE(page.range(), ReportRange::forPreset(Preset::Year, today));
            QCOMPARE(text(page, "reportsRangeLabel"), QString("12 months, to today"));
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
            chooseDays(page);
            QVERIFY(noLengthChecked(page));                // Not whole months
            QCOMPARE(periodName(page), QString("1 Sep – 15 Oct 2026"));
            QCOMPARE(page.range(), ReportRange::custom(QDate(2026, 9, 1), QDate(2026, 10, 15)));
            QCOMPARE(text(page, "reportsRangeLabel"), QString("45 days"));

            // Cancelling keeps what was there
            chip(page, Preset::Month)->click();
            QDate askedFrom;
            page.askCustomRange = [&askedFrom](QDate& from, QDate&) {
                askedFrom = from;
                return false;
            };
            chooseDays(page);
            QCOMPARE(askedFrom, page.range().first);     // Starts from the shown range
            QVERIFY(chip(page, Preset::Month)->isChecked());
            QCOMPARE(page.range().months(), 1);

            page.askCustomRange = [](QDate& from, QDate& to) {
                from = QDate(2026, 3, 1);
                to = QDate(2026, 3, 20);
                return true;
            };
            chooseDays(page);
        }
        ReportsPage again;
        again.setUserId(1);
        QCOMPARE(again.range(), ReportRange::custom(QDate(2026, 3, 1), QDate(2026, 3, 20)));
    }

    void theArrowsStepByTheLength() {
        const QDate today = QDate::currentDate();
        const ReportRange threeMonths = ReportRange::forPreset(Preset::ThreeMonths, today);
        const ReportRange before{threeMonths.first.addMonths(-3), threeMonths.first.addDays(-1)};
        {
            ReportsPage page;
            page.setUserId(1);
            auto* previous = page.findChild<QToolButton*>("previousPeriod");
            auto* next = page.findChild<QToolButton*>("nextPeriod");
            auto* name = page.findChild<QToolButton*>("periodName");
            chip(page, Preset::ThreeMonths)->click();
            QCOMPARE(periodName(page), threeMonths.label());
            QVERIFY(!next->isEnabled());                 // Nothing after this month yet
            const int width = name->width();

            previous->click();
            QCOMPARE(page.range(), before);
            QCOMPARE(periodName(page), before.label());
            QVERIFY(chip(page, Preset::ThreeMonths)->isChecked());   // Still three months long
            QCOMPARE(text(page, "reportsRangeLabel"), QString("3 months"));
            QVERIFY(next->isEnabled());
            QCOMPARE(name->width(), width);              // The arrows stay put
            for (int step = 0; step < 4; ++step) previous->click();
            QCOMPARE(name->width(), width);              // Across a year's end too
            for (int step = 0; step < 4; ++step) next->click();
            QCOMPARE(page.range(), before);
        }
        {
            ReportsPage again;                           // A range stepped back to is remembered as it is
            again.setUserId(1);
            QCOMPARE(again.range(), before);
            QVERIFY(chip(again, Preset::ThreeMonths)->isChecked());
            again.findChild<QToolButton*>("nextPeriod")->click();
            QCOMPARE(again.range(), threeMonths);
        }
        ReportsPage latest;                              // Back at this month, it's the 3 months again
        latest.setUserId(1);
        QCOMPARE(Settings::instance().getReportsRange(), ReportRange::key(Preset::ThreeMonths));
        QCOMPARE(latest.range(), threeMonths);

        // A length starts again from this month
        latest.findChild<QToolButton*>("previousPeriod")->click();
        chip(latest, Preset::Year)->click();
        QCOMPARE(latest.range(), ReportRange::forPreset(Preset::Year, today));
        chip(latest, Preset::Month)->click();
        QCOMPARE(text(latest, "reportsRangeLabel"), QString("1 month, to today"));
    }

    void theStepperGoesByThePagesDate() {
        ReportsPage page;
        page.today = [] { return QDate(2026, 3, 15); };
        page.setUserId(1);                               // Reloads: the months move on to the page's date
        QCOMPARE(periodName(page), ReportRange::forPreset(Preset::SixMonths, QDate(2026, 3, 15)).label());
        chip(page, Preset::ThreeMonths)->click();
        QCOMPARE(periodName(page), QString("January – March 2026"));
        QVERIFY(!page.findChild<QToolButton*>("nextPeriod")->isEnabled());   // April hasn't started

        // A range that hasn't begun isn't "to today"
        page.askCustomRange = [](QDate& from, QDate& to) {
            from = QDate(2026, 5, 1);
            to = QDate(2026, 5, 31);
            return true;
        };
        chooseDays(page);
        QCOMPARE(text(page, "reportsRangeLabel"), QString("1 month"));
        QVERIFY(!page.findChild<PeriodMenu*>()->isVisible());   // The days were asked for, not the menu

        // Cancelling ignores whatever the dialog had
        page.askCustomRange = [](QDate& from, QDate& to) {
            from = QDate(2020, 1, 1);
            to = QDate(2020, 1, 2);
            return false;
        };
        chooseDays(page);
        QCOMPARE(page.range(), ReportRange::custom(QDate(2026, 5, 1), QDate(2026, 5, 31)));
    }

    void theNamesWidthStaysForEveryRange() {
        ReportsPage page;
        page.setUserId(1);
        page.resize(1600, 600);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* name = page.findChild<QToolButton*>("periodName");
        const auto showDays = [&page](const QDate& from, const QDate& to) {
            page.askCustomRange = [from, to](QDate& first, QDate& last) {
                first = from;
                last = to;
                return true;
            };
            chooseDays(page);
            QCoreApplication::processEvents();           // Lays the row out again
        };
        showDays(QDate(2026, 5, 1), QDate(2026, 5, 31));
        const int width = name->width();
        showDays(QDate(2025, 9, 1), QDate(2026, 12, 31));   // As long as a name of months gets
        QCOMPARE(name->text(), QString("September 2025 – December 2026"));
        QCOMPARE(name->width(), width);
        QVERIFY(name->fontMetrics().horizontalAdvance(name->text()) <= name->width());
    }

    void aSingleDayAndCancellingTheDays() {
        ReportsPage page;
        page.setUserId(1);
        page.askCustomRange = [](QDate& from, QDate& to) {
            from = to = QDate(2026, 3, 4);
            return true;
        };
        chooseDays(page);
        QCOMPARE(text(page, "reportsRangeLabel"), QString("1 day"));
        QVERIFY(noLengthChecked(page));
        page.askCustomRange = [](QDate& from, QDate& to) {
            from = QDate(2026, 1, 1);
            to = QDate(2026, 2, 28);                     // Two whole months: no length of its own
            return true;
        };
        chooseDays(page);
        QCOMPARE(text(page, "reportsRangeLabel"), QString("2 months"));
        QVERIFY(noLengthChecked(page));
        page.askCustomRange = [](QDate& from, QDate& to) {
            from = QDate(2025, 1, 1);
            to = QDate(2025, 12, 31);
            return true;
        };
        chooseDays(page);
        QVERIFY(chip(page, Preset::Year)->isChecked());  // Twelve whole months
    }

    void cardsOpenTheirReport() {
        ReportsPage page;
        page.setUserId(1);
        const QList<QAbstractButton*> all = cards(page);
        QCOMPARE(all.size(), 7);
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

    void topPayeesOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Landlord", 90000, today, "", 0, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 3000, today, "", 0, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "", 500, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        QAbstractButton* card = cards(page)[4];
        QVERIFY(card->accessibleDescription().startsWith("Landlord, "));
        QVERIFY(page.findChild<QLabel*>("payeePreview")->text().startsWith("REWE · "));

        card->click();
        const QList<QAbstractButton*> rows = page.findChildren<QAbstractButton*>("payeeRow");
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows[2]->accessibleName(), QString("No payee"));
        QSignalSpy requested(&page, &ReportsPage::payeeTransactionsRequested);
        rows[1]->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested[0][0].toString(), QString("REWE"));
        QCOMPARE(requested[0][2].toDate(), page.range().first);
        rows[2]->click();                                // No payee to search for
        QCOMPARE(requested.size(), 1);
    }

    void reportsFillTheWidthOrAreCentred() {
        constexpr int Wide = 1800;
        constexpr int Centred = ReportsPage::CentredWidth;
        Settings::instance().setReportsCentred(false);   // Centred is the default
        // A small font, set before the page is made: the styled header controls don't take a font
        // set on the page later
        const QFont normal = QApplication::font();
        const auto restoreFont = qScopeGuard([normal] { QApplication::setFont(normal); });
        QFont small = normal;
        small.setPointSize(6);
        QApplication::setFont(small);
        ReportsPage page;
        page.setUserId(1);
        page.resize(Wide, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* column = page.findChild<QWidget*>("reportsColumn");
        auto* header = page.findChild<QWidget*>("reportsWidthButton");      // The header's last
        const auto rightEdge = [&page](QWidget* widget) { return widget->mapTo(&page, QPoint(widget->width(), 0)).x(); };

        // The whole width, the header's choices at its end and an opened report as wide
        QCOMPARE(column->geometry(), page.rect().adjusted(24, 20, -24, -20));
        QCOMPARE(rightEdge(header), Wide - 24);
        cards(page)[1]->click();
        QTRY_COMPARE(rightEdge(page.findChild<QWidget*>("flowDetail")), Wide - 24);

        // Centred: with as much room on either side, the header ending where the report and the
        // cards do (the small font from the start, so the header fits; the test platform's is wide)
        page.findChild<QToolButton*>("reportsBackLink")->click();
        Settings::instance().setReportsCentred(true);
        page.applySettings();
        QTRY_COMPARE(column->width(), Centred);
        QCOMPARE(column->x(), (Wide - Centred) / 2);
        QCOMPARE(rightEdge(header), (Wide + Centred) / 2);
        QTRY_COMPARE(rightEdge(cards(page)[2]), (Wide + Centred) / 2);
        cards(page)[1]->click();
        QTRY_COMPARE(rightEdge(page.findChild<QWidget*>("flowDetail")), (Wide + Centred) / 2);
        // The room on either side isn't needed, so the window can still get narrower
        QCOMPARE(page.minimumSizeHint().width(), column->minimumSizeHint().width() + 48);

        // Wider, still centred, when the header needs more room (a title that needs it, whatever the
        // platform's fonts)
        auto* title = page.findChild<QLabel*>("reportsTitle");
        title->setMinimumWidth(title->minimumSizeHint().width() + Centred + 100 - column->minimumSizeHint().width());
        QTRY_VERIFY(column->width() > Centred);
        QTRY_VERIFY(qAbs(column->x() - (Wide - rightEdge(column))) <= 1);   // Give or take a pixel
        QCOMPARE(column->width(), column->minimumSizeHint().width());

        // And back
        Settings::instance().setReportsCentred(false);
        page.applySettings();
        QTRY_COMPARE(column->width(), Wide - 48);
    }
    void theWidthButtonSwitchesAndIsRemembered() {
        QVERIFY(Settings::instance().isReportsCentred());   // The default
        ReportsPage page;
        page.setUserId(1);
        page.resize(1800, 800);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* column = page.findChild<QWidget*>("reportsColumn");
        auto* button = page.findChild<QToolButton*>("reportsWidthButton");
        QSignalSpy changed(&page, &ReportsPage::widthChanged);
        QCOMPARE(button->toolTip(), QString("Use the full width"));

        button->click();
        QVERIFY(!Settings::instance().isReportsCentred());
        QTRY_COMPARE(column->width(), 1800 - 48);
        QCOMPARE(button->toolTip(), QString("Back to a centred column"));
        QCOMPARE(changed.size(), 1);

        button->click();
        QVERIFY(Settings::instance().isReportsCentred());
        QTRY_VERIFY(column->width() < 1800 - 48);
        QCOMPARE(button->toolTip(), QString("Use the full width"));
        QCOMPARE(changed.size(), 2);

        // Changed in the settings: the button follows
        Settings::instance().setReportsCentred(false);
        page.applySettings();
        QCOMPARE(button->toolTip(), QString("Back to a centred column"));
    }

    void longNamesDontWidenTheirCard() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        const QString longest = "PAYPAL *SOMESHOP EUROPE S.A.R.L. ET CIE S.C.A. LUXEMBOURG 35314369001 " + QString(40, 'X');
        const QString second = "AMAZON PAYMENTS EUROPE S.C.A. MARKETPLACE " + QString(40, 'Y');
        TransactionDAO().addTransaction(Transaction(-1, giro, longest, 90000, today, "", 0, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, second, 3000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        page.resize(1200, 700);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        const QList<QAbstractButton*> all = cards(page);
        for (const QAbstractButton* card : all) {
            QCOMPARE(card->width(), all.first()->width());
            QVERIFY(card->minimumSizeHint().width() < 300);
        }

        // The names are cut short, and in full as the card's tool tip
        QAbstractButton* payeeCard = all[4];
        QHelpEvent help(QEvent::ToolTip, QPoint(10, 10), payeeCard->mapToGlobal(QPoint(10, 10)));
        QApplication::sendEvent(payeeCard, &help);
        QVERIFY(QToolTip::text().contains(longest));
        QVERIFY(QToolTip::text().contains(second));
        QToolTip::hideText();
        QTRY_VERIFY(!QToolTip::isVisible());
        QHelpEvent onBudget(QEvent::ToolTip, QPoint(10, 10), all[5]->mapToGlobal(QPoint(10, 10)));
        QApplication::sendEvent(all[5], &onBudget);
        QVERIFY(!QToolTip::isVisible());
    }

    void budgetVsActualInTabs() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        const QDate first(today.year(), today.month(), 1);
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, first.addMonths(-2), 10000));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 4000, today, "", groceries, "expense"));
        Settings::instance().setReportsBudgetView(0);

        // A month: all together as its pace, or each budget day by day
        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        page.openReport(ReportsPage::Report::BudgetVsActual);
        QVERIFY(viewChip(page, "together")->isChecked());
        QVERIFY(!viewChip(page, "grid"));
        QCOMPARE(shownView(page)->objectName(), QString("budgetChart"));
        QCOMPARE(shownView(page)->minimumHeight(), 280);
        viewChip(page, "each")->click();
        QCOMPARE(shownView(page)->objectName(), QString("budgetEachView"));
        QCOMPARE(Settings::instance().getReportsBudgetView(), 1);
        auto* lines = page.findChild<BudgetLinesChart*>();
        QVERIFY(lines);
        QCOMPARE(lines->lastDay(), today.day());
        QVERIFY(page.findChild<QLabel*>("budgetLegend")->text().contains("Groceries"));
        QSignalSpy requested(&page, &ReportsPage::transactionsRequested);
        emit lines->budgetClicked(groceries);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.last()[0].toInt(), groceries);
        QCOMPARE(requested.last()[1].toInt(), int(ReportsPage::ALL_ACCOUNTS));
        QCOMPARE(requested.last()[2].toDate(), first);
        QCOMPARE(requested.last()[3].toDate(), first.addMonths(1).addDays(-1));

        // Several months: the bars, each budget month by month, and the grid; the view is remembered
        chip(page, Preset::ThreeMonths)->click();
        QVERIFY(viewChip(page, "grid"));
        QVERIFY(viewChip(page, "each")->isChecked());
        QVERIFY(!page.findChild<BudgetLinesChart*>());
        auto* months = page.findChild<BudgetMonthsChart*>();
        auto* grid = page.findChild<BudgetGrid*>();
        QVERIFY(months && grid);
        QCOMPARE(shownView(page)->objectName(), QString("budgetEachView"));
        viewChip(page, "grid")->click();
        QCOMPARE(shownView(page), grid);
        QVERIFY(grid->minimumHeight() < 280);                  // Only as tall as its rows
        QCOMPARE(page.findChild<QWidget*>("budgetEachView")->minimumHeight(), 280);
        QCOMPARE(Settings::instance().getReportsBudgetView(), 2);
        const ReportRange lastMonth{first.addMonths(-1), first.addDays(-1)};
        emit grid->monthClicked(groceries, lastMonth);
        QCOMPARE(requested.last()[2].toDate(), lastMonth.first);
        QCOMPARE(requested.last()[3].toDate(), lastMonth.last);
        emit months->monthClicked(groceries, ReportRange{first, first.addMonths(1).addDays(-1)});
        QCOMPARE(requested.size(), 3);
        QCOMPARE(requested.last()[2].toDate(), first);

        // Back to a month there's no grid: each budget, and the grid stays remembered for later
        chip(page, Preset::Month)->click();
        QVERIFY(viewChip(page, "each")->isChecked());
        QCOMPARE(shownView(page)->objectName(), QString("budgetEachView"));
        QCOMPARE(Settings::instance().getReportsBudgetView(), 2);
        Settings::instance().setReportsBudgetView(0);
    }

    void theBudgetPageOpensItsMonth() {
        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::ThreeMonths)->click();
        const QDate today = QDate::currentDate();
        page.openBudgetMonth(today);
        QCOMPARE(page.openedReport(), std::optional(ReportsPage::Report::BudgetVsActual));
        QCOMPARE(page.range(), ReportRange::forPreset(Preset::Month, today));
        QVERIFY(chip(page, Preset::Month)->isChecked());

        const QDate earlier = QDate(today.year(), today.month(), 1).addMonths(-2);
        page.openBudgetMonth(earlier.addDays(9));
        QCOMPARE(page.range(), ReportRange({earlier, earlier.addMonths(1).addDays(-1)}));
        QVERIFY(chip(page, Preset::Month)->isChecked());   // A month long, two months back
        QVERIFY(page.findChild<QToolButton*>("previousPeriod")->isEnabled());
        chip(page, Preset::SixMonths)->click();
    }

    void budgetVsActualOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();

        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        QAbstractButton* card = cards(page)[5];
        QCOMPARE(card->accessibleDescription(), QString("No budgets in this range"));

        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, today, 10000));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 4000, today, "", groceries, "expense"));
        page.reload();
        QVERIFY(card->accessibleDescription().startsWith("1 of 1 kept, "));

        card->click();
        QVERIFY(!page.findChild<QLabel*>("budgetNote"));
        const QList<QAbstractButton*> rows = page.findChildren<QAbstractButton*>("budgetRow");
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0]->accessibleName(), QString("Groceries"));
        QVERIFY(rows[0]->accessibleDescription().endsWith("40% used"));

        // How the month goes against an even pace, above its chart
        const Settings& settings = Settings::instance();
        auto* chart = page.findChild<QWidget*>("budgetChart");
        QVERIFY(chart);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        const auto top = [&page](QWidget* widget) { return widget->mapTo(&page, QPoint()).y(); };
        QTRY_VERIFY(top(chart) > top(page.findChild<QLabel*>("budgetSummary")));               // Below the summary,
        QVERIFY(top(chart) < top(page.findChild<QAbstractButton*>("budgetRow")));               // above the list
        const QString summary = text(page, "budgetSummary");
        if (today.day() == today.daysInMonth()) {
            QCOMPARE(summary, QString("%1 of %2 spent").arg(settings.formatCurrency(4000), settings.formatCurrency(10000)));
        } else {
            const Cents difference = 4000 - 10000 * today.day() / today.daysInMonth();
            QVERIFY2(summary.startsWith(QString("%1 spent by the %2").arg(settings.formatCurrency(4000)).arg(today.day())), qPrintable(summary));
            QVERIFY2(summary.endsWith(difference == 0 ? QString(", right on an even pace")
                                                      : QString(", %1 %2 an even pace").arg(settings.formatCurrency(qAbs(difference)),
                                                                                           difference > 0 ? "over" : "under")),
                     qPrintable(summary));
        }

        // Several months: how many were over
        chip(page, Preset::ThreeMonths)->click();
        QCOMPARE(text(page, "budgetSummary"), QString("Within the budgets every month"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 7000, today, "", groceries, "expense"));
        page.reload();
        QCOMPARE(text(page, "budgetSummary"), QString("Over the budgets in 1 of 1 month"));
        chip(page, Preset::Month)->click();

        // The chosen account doesn't apply, and that's said
        auto* combo = page.findChild<QComboBox*>("reportsAccountCombo");
        combo->setCurrentIndex(combo->findData(giro));
        emit combo->activated(combo->currentIndex());
        QVERIFY(page.findChild<QLabel*>("budgetNote"));

        // On to its trend
        page.findChildren<QAbstractButton*>("budgetRow").first()->click();
        QCOMPARE(page.openedReport(), ReportsPage::Report::CategoryTrend);
        QCOMPARE(page.trendCategory(), groceries);
    }

    void aFinishedMonthSaysWhatWasSpent() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        const QDate lastMonth = QDate(today.year(), today.month(), 1).addMonths(-1);
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, lastMonth, 10000));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 2500, lastMonth.addDays(3), "", groceries, "expense"));

        ReportsPage page;
        page.setUserId(1);
        page.openBudgetMonth(lastMonth);
        const Settings& settings = Settings::instance();
        QCOMPARE(text(page, "budgetSummary"), QString("%1 of %2 spent").arg(settings.formatCurrency(2500), settings.formatCurrency(10000)));
    }

    void onItsLastDayAMonthSaysWhatWasSpent() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate march(2026, 3, 1);
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, march, 31000));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 2500, QDate(2026, 3, 4), "", groceries, "expense"));
        const Settings& settings = Settings::instance();

        ReportsPage page;
        page.today = [] { return QDate(2026, 3, 30); };
        page.setUserId(1);
        page.openBudgetMonth(march);
        QVERIFY(chip(page, Preset::Month)->isChecked());  // March is this month here
        QVERIFY(text(page, "budgetSummary").startsWith(QString("%1 spent by the 30th").arg(settings.formatCurrency(2500))));

        page.today = [] { return QDate(2026, 3, 31); };   // Nothing left to pace
        page.openBudgetMonth(march);
        QCOMPARE(text(page, "budgetSummary"), QString("%1 of %2 spent").arg(settings.formatCurrency(2500), settings.formatCurrency(31000)));
    }

    void withOnlyMoneyBackTheTrendHasNoCategory() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 3000, QDate::currentDate(), "", query.value(0).toInt(), "income"));
        ReportsPage page;
        page.setUserId(1);
        QCOMPARE(page.trendCategory(), std::nullopt);
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

    // --- Drawing and the mouse: painting code only runs when something is really drawn ---

    static void showOnScreen(ReportsPage& page) {
        page.setAttribute(Qt::WA_DontShowOnScreen);
        page.resize(1100, 800);
        page.show();
        QCoreApplication::processEvents();
    }
    static void moveMouse(QWidget* widget, QPoint position) {
        QMouseEvent event(QEvent::MouseMove, QPointF(position), widget->mapToGlobal(QPointF(position)),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    }
    static void leave(QWidget* widget) {
        QEvent event(QEvent::Leave);
        QApplication::sendEvent(widget, &event);
    }
    static void releaseMouse(QWidget* widget, QPoint position, Qt::MouseButton button) {
        QMouseEvent event(QEvent::MouseButtonRelease, QPointF(position), widget->mapToGlobal(QPointF(position)),
                          button, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    }
    // In the last of three months' bars, or weeks' for a month
    static QPoint inLastBar(QWidget* chart, int bars) {
        return QPoint(chart->width() - chart->width() / (2 * bars), chart->height() / 2);
    }

    void everyReportDraws_data() {
        QTest::addColumn<int>("preset");
        QTest::addColumn<bool>("withData");
        for (const bool withData : {true, false}) {
            for (const Preset preset : {Preset::Month, Preset::Year}) {
                QTest::addRow("%s, %s", qPrintable(ReportRange::key(preset)), withData ? "with data" : "empty")
                    << int(preset) << withData;
            }
        }
    }

    void everyReportDraws() {
        QFETCH(int, preset);
        QFETCH(bool, withData);
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        if (withData) {
            const QDate today = QDate::currentDate();
            // Below zero, over budget, and a name long enough to be cut off
            TransactionDAO().addTransaction(Transaction(-1, giro, QString(300, 'x'), 90000, today, "", groceries, "expense"));
            TransactionDAO().addTransaction(Transaction(-1, giro, "Employer", 20000, today.addMonths(-2), "", 0, "income"));
            QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, today, 1000));
        }

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset(preset))->click();
        QVERIFY(!page.grab().isNull());
        for (int card = 0; card < cards(page).size(); ++card) {
            cards(page)[card]->click();
            QCoreApplication::processEvents();
            QVERIFY(!page.grab().isNull());
            page.findChild<QToolButton*>("reportsBackLink")->click();
        }
    }

    void moneyFlowOnTheCardAndInDetail() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Employer", 200000, today, "", 0, "income"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 50000, today, "", groceries, "expense"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 10000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset::Month)->click();
        QAbstractButton* card = cards(page)[6];
        QCOMPARE(card->accessibleName(), QString("Money flow"));
        QCOMPARE(card->accessibleDescription(), QString("Groceries, %1, 25% of the income").arg(Settings::instance().formatSpending(50000)));

        // The diagram, and where it went as rows
        card->click();
        QCoreApplication::processEvents();
        const QList<QAbstractButton*> rows = page.findChildren<QAbstractButton*>("moneyFlowRow");
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0]->accessibleName(), QString("Groceries"));
        QCOMPARE(rows[1]->accessibleName(), QString("No category"));
        QVERIFY(page.findChild<QWidget*>("moneyFlowKept"));
        QVERIFY(!page.findChild<QWidget*>("moneyFlowFromSavings"));
        QSignalSpy requested(&page, &ReportsPage::transactionsRequested);
        rows[0]->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested[0][0].toInt(), groceries);
        QCOMPARE(requested[0][2].toDate(), page.range().first);

        // A band says what it is and leads to its transactions too: down the right-hand parts
        auto* chart = page.findChild<QWidget*>("moneyFlowChart");
        const int partsX = chart->width() - qMin(220, chart->width() * 3 / 10) - 5;
        const QImage plain = chart->grab().toImage();
        int groceriesY = -1;
        for (int y = 0; y < chart->height() && groceriesY < 0; y += 2) {
            moveMouse(chart, QPoint(partsX, y));
            if (QToolTip::text().startsWith("Groceries\n")) groceriesY = y;
        }
        QVERIFY(groceriesY >= 0);
        QVERIFY(QToolTip::text().contains("25% of the income"));
        releaseMouse(chart, QPoint(partsX, groceriesY), Qt::LeftButton);
        QCOMPARE(requested.size(), 2);
        QCOMPARE(requested[1][0].toInt(), groceries);
        QVERIFY(chart->grab().toImage() != plain);       // The band stands out until the mouse leaves
        leave(chart);
        QCOMPARE(chart->grab().toImage(), plain);

        // More out than in: the rest from savings, nothing kept
        TransactionDAO().addTransaction(Transaction(-1, giro, "Landlord", 300000, today, "", 0, "expense"));
        page.reload();
        QVERIFY(page.findChild<QWidget*>("moneyFlowFromSavings"));
        QVERIFY(!page.findChild<QWidget*>("moneyFlowKept"));
    }

    void spendingBarsCanBeRed() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 50000, QDate::currentDate(), "", 0, "expense"));
        Settings::instance().setExpensesInRed(false);

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        cards(page)[1]->click();
        QCoreApplication::processEvents();
        auto* chart = page.findChild<QWidget*>("flowChart");
        auto* preview = page.findChild<QWidget*>("flowPreview");
        const QImage plain = chart->grab().toImage();
        const QImage plainPreview = preview->grab().toImage();
        Settings::instance().setExpensesInRed(true);
        QVERIFY(chart->grab().toImage() != plain);
        QVERIFY(preview->grab().toImage() != plainPreview);  // The card's too
        Settings::instance().setExpensesInRed(false);
    }

    void hoveringTheFlowChartShowsAPeriodsNumbers() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Employer", 200000, today, "", 0, "income"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 50000, today, "", 0, "expense"));

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset::ThreeMonths)->click();
        cards(page)[1]->click();
        QCoreApplication::processEvents();
        auto* chart = page.findChild<QWidget*>("flowChart");
        const QImage plain = chart->grab().toImage();

        moveMouse(chart, inLastBar(chart, 3));
        const Settings& settings = Settings::instance();
        QVERIFY2(QToolTip::text().contains("Income " + settings.formatCurrency(200000)), qPrintable(QToolTip::text()));
        QVERIFY(QToolTip::text().contains("Net " + settings.formatCurrency(150000, true)));
        QVERIFY(chart->grab().toImage() != plain);       // The period is highlighted

        leave(chart);
        QCOMPARE(chart->grab().toImage(), plain);
    }

    void hoveringTheBalanceChartShowsADaysBalance() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Start", 100000, today.addMonths(-4), "", 0, "income"));

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset::Month)->click();
        cards(page)[2]->click();
        QCoreApplication::processEvents();
        auto* chart = page.findChild<QWidget*>("balanceChart");
        const QImage plain = chart->grab().toImage();

        moveMouse(chart, QPoint(90, chart->height() / 2));   // The first day, right of the amounts
        QVERIFY2(QToolTip::text().contains(QLocale(QLocale::English).toString(QDate(today.year(), today.month(), 1), "d MMMM yyyy")),
                 qPrintable(QToolTip::text()));
        QVERIFY(QToolTip::text().contains(Settings::instance().formatCurrency(100000)));
        QVERIFY(chart->grab().toImage() != plain);       // Its day is marked

        moveMouse(chart, QPoint(10, chart->height() / 2));   // Over the amounts: no day
        QCOMPARE(chart->grab().toImage(), plain);
    }

    void clickingATrendBarShowsThatPeriodsTransactions() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "REWE", 3000, today, "", groceries, "expense"));

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset::ThreeMonths)->click();
        cards(page)[3]->click();
        QCoreApplication::processEvents();
        auto* chart = page.findChild<QWidget*>("trendChart");
        const QImage plain = chart->grab().toImage();
        QSignalSpy requested(&page, &ReportsPage::transactionsRequested);

        const QPoint lastBar = inLastBar(chart, 3);
        moveMouse(chart, lastBar);
        QVERIFY2(QToolTip::text().contains("1 transaction\nClick to show them"), qPrintable(QToolTip::text()));
        QVERIFY(chart->grab().toImage() != plain);

        releaseMouse(chart, lastBar, Qt::RightButton);   // Only the left button
        QCOMPARE(requested.size(), 0);
        releaseMouse(chart, QPoint(lastBar.x(), chart->height() + 100), Qt::LeftButton);   // Beside the bars
        QCOMPARE(requested.size(), 0);

        releaseMouse(chart, lastBar, Qt::LeftButton);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested[0][0].toInt(), groceries);
        QCOMPARE(requested[0][2].toDate(), QDate(today.year(), today.month(), 1));

        leave(chart);
        QCOMPARE(chart->grab().toImage(), plain);
    }

    void enterOpensARowLikeAClick() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 1000, QDate::currentDate(), "", 0, "expense"));
        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        cards(page)[0]->click();
        QCoreApplication::processEvents();
        QTest::keyClick(page.findChildren<QAbstractButton*>("spendingRow").first(), Qt::Key_Return);
        QCOMPARE(page.openedReport(), ReportsPage::Report::CategoryTrend);
    }

    void rowsLightUpUnderTheMouseAndIgnoreOtherKeys() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 1000, QDate::currentDate(), "", 0, "expense"));
        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        QTest::keyClick(cards(page)[0], Qt::Key_A);
        QCOMPARE(page.openedReport(), std::nullopt);
        cards(page)[0]->click();
        QCoreApplication::processEvents();
        auto* row = page.findChildren<QAbstractButton*>("spendingRow").first();
        const QImage plain = row->grab().toImage();
        row->setAttribute(Qt::WA_UnderMouse);
        QVERIFY(row->grab().toImage() != plain);
        QTest::keyClick(row, Qt::Key_A);
        QCOMPARE(page.openedReport(), ReportsPage::Report::SpendingByCategory);
    }

    void chartsLetGoWhenTheMouseLeavesTheirBars() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        TransactionDAO().addTransaction(Transaction(-1, giro, "Employer", 200000, today, "", 0, "income"));
        TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", 50000, today, "", 0, "expense"));
        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        chip(page, Preset::ThreeMonths)->click();

        const QList<QPair<int, const char*>> charts = {{1, "flowChart"}, {2, "balanceChart"}, {3, "trendChart"}};
        for (const auto& [card, name] : charts) {
            cards(page)[card]->click();
            QCoreApplication::processEvents();
            auto* chart = page.findChild<QWidget*>(name);
            const QImage plain = chart->grab().toImage();
            moveMouse(chart, inLastBar(chart, 3));
            moveMouse(chart, QPoint(0, 0));
            QCOMPARE(chart->grab().toImage(), plain);
            moveMouse(chart, inLastBar(chart, 3));
            leave(chart);
            QCOMPARE(chart->grab().toImage(), plain);
            page.findChild<QToolButton*>("reportsBackLink")->click();
        }
    }

    void theSpendingCardComparesWithTheMonthBefore() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        const QDate today = QDate::currentDate();
        const QDate before = QDate(today.year(), today.month(), 1).addMonths(-1);
        const auto spend = [&](const QDate& date, Cents cents) {
            QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "Shop", cents, date, "", 0, "expense")) > 0);
        };
        const auto cardSays = [&]() {
            ReportsPage page;
            page.setUserId(1);
            chip(page, Preset::Month)->click();
            return cards(page).first()->accessibleDescription();
        };
        spend(today, 3000);
        spend(before, 2000);
        QVERIFY2(cardSays().contains("+50% vs. "), qPrintable(cardSays()));
        spend(before, 1000);
        QVERIFY2(cardSays().contains("±0% vs. "), qPrintable(cardSays()));
        spend(before, 3000);
        QVERIFY2(cardSays().contains("−50% vs. "), qPrintable(cardSays()));
    }

    void payeesBeyondTheLargestAreSummedUp() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        for (int i = 1; i <= 22; ++i) {
            TransactionDAO().addTransaction(Transaction(-1, giro, QString("Shop %1").arg(i), 100 * i, QDate::currentDate(), "", 0, "expense"));
        }
        ReportsPage page;
        page.setUserId(1);
        chip(page, Preset::Month)->click();
        cards(page)[4]->click();
        QCOMPARE(page.findChildren<QAbstractButton*>("payeeRow").size(), 20);
        QCOMPARE(page.findChild<QWidget*>("payeeRest")->accessibleName(), QString("2 other payees"));
    }

    void aCategoryWithoutSpendingStillDraws() {
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        TransactionDAO().addTransaction(Transaction(-1, giro, "Kiosk", 1000, QDate::currentDate(), "", 0, "expense"));
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Rent'") && query.next());
        const int rent = query.value(0).toInt();

        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        cards(page)[3]->click();
        auto* picker = page.findChild<QComboBox*>("trendCategoryCombo");
        picker->setCurrentIndex(picker->findData(rent));
        emit picker->activated(picker->currentIndex());
        QCOMPARE(page.trendCategory(), rent);
        QCoreApplication::processEvents();
        QVERIFY(!page.findChild<QWidget*>("trendChart")->grab().isNull());
    }

    void enterOpensACard() {
        ReportsPage page;
        page.setUserId(1);
        showOnScreen(page);
        QTest::keyClick(cards(page)[1], Qt::Key_Return);
        QCOMPARE(page.openedReport(), ReportsPage::Report::IncomeVsSpending);
    }

    void theCustomRangeDialogAsksForBothDays() {
        ReportsPage page;
        page.setUserId(1);
        const ReportRange before = page.range();
        const auto answerDialog = [](QDialogButtonBox::StandardButton button) {
            QTimer::singleShot(0, qApp, [button]() {
                QWidget* dialog = QApplication::activeModalWidget();
                const QList<QDateEdit*> edits = dialog->findChildren<QDateEdit*>();
                edits[0]->setDate(QDate(2026, 2, 3));
                edits[1]->setDate(QDate(2026, 4, 5));
                dialog->findChild<QDialogButtonBox*>()->button(button)->click();
            });
        };

        answerDialog(QDialogButtonBox::Cancel);
        chooseDays(page);
        QCOMPARE(page.range(), before);

        answerDialog(QDialogButtonBox::Ok);
        chooseDays(page);
        QCOMPARE(page.range(), ReportRange::custom(QDate(2026, 2, 3), QDate(2026, 4, 5)));
    }
};

QTEST_MAIN(TestReportsPage)
#include "test_reportspage.moc"
