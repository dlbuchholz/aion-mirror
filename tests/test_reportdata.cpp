#include <QtTest>
#include "account.h"
#include "category.h"
#include "report.h"
#include "tests/test_db_fixture.h"

using Preset = ReportRange::Preset;

// The numbers behind the reports, from the database
class TestReportData : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;
    int savings = -1;
    int groceries = 0;
    int rent = 0;
    const QDate today = QDate(2026, 10, 29);

    static int categoryId(const QString& name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    int add(int account, Cents cents, const QDate& date, int category, const char* type) {
        return TransactionDAO().addTransaction(Transaction(-1, account, "Shop", cents, date, "", category, type));
    }

    static const CategorySpent* find(const SpendingReport& report, const QString& name) {
        for (const CategorySpent& spent : report.categories) {
            if (spent.categoryName == name) return &spent;
        }
        return nullptr;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        savings = AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0);
        groceries = categoryId("Groceries");
        rent = categoryId("Rent");
        QVERIFY(giro > 0 && savings > 0 && groceries > 0 && rent > 0);
    }
    void cleanup() { db.close(); }

    void spendingPerCategoryLargestFirst() {
        const ReportRange threeMonths = ReportRange::forPreset(Preset::ThreeMonths, today);   // August – October
        add(giro, 89000, QDate(2026, 8, 3), rent, "expense");
        add(giro, 89000, QDate(2026, 10, 3), rent, "expense");
        add(giro, 31200, QDate(2026, 9, 1), groceries, "expense");
        add(savings, 4640, QDate(2026, 10, 31), groceries, "expense");   // Later this month counts too
        add(giro, 5000, QDate(2026, 10, 5), groceries, "income");        // A refund lowers it
        add(giro, 1250, QDate(2026, 10, 4), 0, "expense");
        add(giro, 7000, QDate(2026, 7, 31), rent, "expense");            // Before the range
        add(giro, 285000, QDate(2026, 10, 15), categoryId("Salary"), "income");

        const SpendingReport report = Reports::spendingByCategory(1, -1, threeMonths, today);
        QCOMPARE(report.categories.size(), 3);
        QCOMPARE(report.categories[0].categoryName, QString("Rent"));
        QCOMPARE(report.categories[0].spentCents, Cents(178000));
        QCOMPARE(report.categories[1].categoryName, QString("Groceries"));
        QCOMPARE(report.categories[1].spentCents, Cents(31200 + 4640 - 5000));
        QCOMPARE(report.categories[2].categoryName, QString("No category"));
        QCOMPARE(report.categories[2].categoryId, 0);
        QCOMPARE(report.totalCents, Cents(178000 + 30840 + 1250));
    }

    void transfersAndBalanceAdjustmentsAreLeftOut() {
        add(giro, 50000, QDate(2026, 10, 10), categoryId(TransferCategory), "expense");
        add(giro, 12000, QDate(2026, 10, 1), categoryId(BalanceCorrectionCategory), "expense");
        add(giro, 900, QDate(2026, 10, 2), groceries, "expense");
        const SpendingReport report = Reports::spendingByCategory(1, -1, ReportRange::forPreset(Preset::Month, today), today);
        QCOMPARE(report.categories.size(), 1);
        QCOMPARE(report.totalCents, Cents(900));
    }

    void comparesWithTheRangeBefore() {
        // October so far against September up to the 29th
        add(giro, 10000, QDate(2026, 9, 29), groceries, "expense");
        add(giro, 4000, QDate(2026, 9, 30), groceries, "expense");       // After the same day
        add(giro, 3000, QDate(2026, 9, 2), rent, "expense");             // Only before: not listed
        add(giro, 10800, QDate(2026, 10, 1), groceries, "expense");
        const SpendingReport report = Reports::spendingByCategory(1, -1, ReportRange::forPreset(Preset::Month, today), today);
        QCOMPARE(report.before, ReportRange({QDate(2026, 9, 1), QDate(2026, 9, 29)}));
        QCOMPARE(report.categories.size(), 1);
        QCOMPARE(report.categories[0].beforeCents, Cents(10000));
        QCOMPARE(report.beforeTotalCents, Cents(13000));
        QCOMPARE(report.totalCents, Cents(10800));
        QVERIFY(!find(report, "Rent"));
    }

    void oneAccountOnly() {
        add(giro, 1000, QDate(2026, 10, 2), groceries, "expense");
        add(savings, 2500, QDate(2026, 10, 2), rent, "expense");
        const ReportRange month = ReportRange::forPreset(Preset::Month, today);
        const SpendingReport report = Reports::spendingByCategory(1, savings, month, today);
        QCOMPARE(report.totalCents, Cents(2500));
        QVERIFY(find(report, "Rent"));
        QVERIFY(!find(report, "Groceries"));
        QCOMPARE(Reports::spendingByCategory(1, -1, month, today).totalCents, Cents(3500));
    }

    void incomeAndSpendingPerMonth() {
        const ReportRange threeMonths = ReportRange::forPreset(Preset::ThreeMonths, today);   // August – October
        const int salary = categoryId("Salary");
        add(giro, 300000, QDate(2026, 8, 28), salary, "income");
        add(giro, 89000, QDate(2026, 8, 3), rent, "expense");
        add(giro, 300000, QDate(2026, 10, 28), salary, "income");
        add(giro, 20000, QDate(2026, 10, 2), groceries, "expense");
        add(giro, 5000, QDate(2026, 10, 9), groceries, "income");        // Refund: less spending, not income
        add(savings, 1500, QDate(2026, 10, 31), 0, "expense");
        add(giro, 50000, QDate(2026, 10, 10), categoryId(TransferCategory), "expense");   // Left out
        add(giro, 9999, QDate(2026, 7, 31), rent, "expense");            // Before the range

        const FlowReport report = Reports::incomeVsSpending(1, -1, threeMonths);
        QCOMPARE(report.periods.size(), 3);
        QCOMPARE(report.periods[0].period, ReportRange({QDate(2026, 8, 1), QDate(2026, 8, 31)}));
        QCOMPARE(report.periods[0].incomeCents, Cents(300000));
        QCOMPARE(report.periods[0].spendingCents, Cents(89000));
        QCOMPARE(report.periods[1].incomeCents, Cents(0));               // Empty months are there too
        QCOMPARE(report.periods[1].spendingCents, Cents(0));
        QCOMPARE(report.periods[2].incomeCents, Cents(300000));
        QCOMPARE(report.periods[2].spendingCents, Cents(20000 - 5000 + 1500));
        QCOMPARE(report.incomeCents, Cents(600000));
        QCOMPARE(report.spendingCents, Cents(89000 + 16500));
        QCOMPARE(report.keptPercent(), std::optional<int>(qRound((600000.0 - 105500.0) / 6000.0)));

        QCOMPARE(Reports::incomeVsSpending(1, savings, threeMonths).spendingCents, Cents(1500));
    }

    void shortRangesGoByWeek() {
        add(giro, 1000, QDate(2026, 10, 4), groceries, "expense");       // Sunday: the first, partial week
        add(giro, 2000, QDate(2026, 10, 5), groceries, "expense");       // Monday: the next
        const FlowReport report = Reports::incomeVsSpending(1, -1, ReportRange::forPreset(Preset::Month, today));
        QCOMPARE(report.periods.size(), 5);
        QCOMPARE(report.periods[0].spendingCents, Cents(1000));
        QCOMPARE(report.periods[1].spendingCents, Cents(2000));
        QVERIFY(!report.keptPercent());                                  // No income
        QCOMPARE(report.periods[0].period.weekName(), QString("Week 40"));
        QCOMPARE(report.periods[1].period.weekName(), QString("Week 41"));
    }

    void keptOnlyWhenSomethingWasKept() {
        QCOMPARE(Reports::keptPercent(272774, 229096), std::optional<int>(84));
        QCOMPARE(Reports::keptPercent(1000, 0), std::optional<int>(0));
        QVERIFY(!Reports::keptPercent(1, -13773));                       // More went out: no share
        QVERIFY(!Reports::keptPercent(0, -4268));
    }

    void balanceDayByDayUpToToday() {
        add(giro, 100000, QDate(2026, 7, 1), categoryId(InitialBalanceCategory), "income");   // Before the range
        add(giro, 20000, QDate(2026, 10, 3), groceries, "expense");
        add(giro, 5000, QDate(2026, 10, 3), groceries, "income");
        add(giro, 30000, QDate(2026, 10, 10), categoryId(TransferCategory), "expense");      // Transfers move it too
        add(savings, 30000, QDate(2026, 10, 10), categoryId(TransferCategory), "income");
        add(giro, 7000, QDate(2026, 10, 30), rent, "expense");           // Planned, after today

        const ReportRange month = ReportRange::forPreset(Preset::Month, today);
        const BalanceReport all = Reports::balanceOverTime(1, -1, month, today);
        QCOMPARE(all.startCents, Cents(100000));
        QCOMPARE(all.days.size(), 29);                                   // 1 to 29 October
        QCOMPARE(all.days.first().balanceCents, Cents(100000));
        QCOMPARE(all.days[2].date, QDate(2026, 10, 3));
        QCOMPARE(all.days[2].balanceCents, Cents(85000));
        QCOMPARE(all.endCents(), Cents(85000));                          // The transfer evens out
        QCOMPARE(all.changeCents(), Cents(-15000));
        QCOMPARE(all.balanceOn(QDate(2026, 10, 9)), Cents(85000));
        QCOMPARE(all.balanceOn(QDate(2026, 9, 30)), Cents(100000));

        const BalanceReport giroOnly = Reports::balanceOverTime(1, giro, month, today);
        QCOMPARE(giroOnly.endCents(), Cents(55000));
        QCOMPARE(Reports::balanceOverTime(1, savings, month, today).endCents(), Cents(30000));

        // A range that's over runs to its end
        const BalanceReport september = Reports::balanceOverTime(1, -1, {QDate(2026, 9, 1), QDate(2026, 9, 30)}, today);
        QCOMPARE(september.days.size(), 30);
        QCOMPARE(september.changeCents(), Cents(0));
    }

    void oneCategoryMonthByMonth() {
        const ReportRange threeMonths = ReportRange::forPreset(Preset::ThreeMonths, today);   // August – October
        add(giro, 40000, QDate(2026, 8, 3), groceries, "expense");
        add(giro, 2000, QDate(2026, 8, 20), groceries, "expense");
        add(savings, 38000, QDate(2026, 10, 5), groceries, "expense");
        add(giro, 3000, QDate(2026, 10, 6), groceries, "income");        // A refund lowers it
        add(giro, 89000, QDate(2026, 10, 3), rent, "expense");           // Another category
        add(giro, 9999, QDate(2026, 7, 30), groceries, "expense");       // Before the range

        const CategoryTrend trend = Reports::categoryTrend(1, -1, groceries, threeMonths, today);
        QCOMPARE(trend.categoryName, QString("Groceries"));
        QCOMPARE(trend.periods.size(), 3);
        QCOMPARE(trend.periods[0].spentCents, Cents(42000));
        QCOMPARE(trend.periods[0].count, 2);
        QCOMPARE(trend.periods[1].spentCents, Cents(0));
        QCOMPARE(trend.periods[2].spentCents, Cents(35000));
        QCOMPARE(trend.totalCents, Cents(77000));
        QCOMPARE(trend.count, 4);
        QCOMPARE(trend.averageCents, Cents(77000 / 3));

        QCOMPARE(Reports::categoryTrend(1, giro, groceries, threeMonths, today).totalCents, Cents(42000 - 3000));

        // Uncategorised ones are a category of their own
        add(giro, 1250, QDate(2026, 9, 4), 0, "expense");
        const CategoryTrend none = Reports::categoryTrend(1, -1, 0, threeMonths, today);
        QCOMPARE(none.categoryName, QString("No category"));
        QCOMPARE(none.totalCents, Cents(1250));
    }

    void trendAveragesOnlyOverWhatHasBegun() {
        // Four weeks from Monday 26 October; by the 29th only the first has begun
        const ReportRange later{QDate(2026, 10, 26), QDate(2026, 11, 22)};
        add(giro, 4000, QDate(2026, 10, 27), groceries, "expense");
        const CategoryTrend trend = Reports::categoryTrend(1, -1, groceries, later, today);
        QCOMPARE(trend.periods.size(), 4);
        QCOMPARE(trend.averageCents, Cents(4000));
    }

    void payeesLargestFirstWithSpellingsTogether() {
        const ReportRange month = ReportRange::forPreset(Preset::Month, today);
        const auto addFrom = [this](int account, const char* payee, Cents cents, int category, const char* type) {
            return TransactionDAO().addTransaction(Transaction(-1, account, payee, cents, QDate(2026, 10, 5), "", category, type));
        };
        addFrom(giro, "REWE", 3000, groceries, "expense");
        addFrom(giro, "REWE", 2000, groceries, "expense");
        addFrom(savings, "Rewe ", 1000, groceries, "expense");           // Same payee
        addFrom(giro, "REWE", 500, groceries, "income");                 // A refund lowers it
        addFrom(giro, "Landlord", 89000, rent, "expense");
        addFrom(giro, "", 700, 0, "expense");
        addFrom(giro, "Employer", 300000, categoryId("Salary"), "income");   // Not spending
        addFrom(giro, "Savings", 50000, categoryId(TransferCategory), "expense");   // Left out

        const PayeeReport report = Reports::topPayees(1, -1, month);
        QCOMPARE(report.payees.size(), 3);
        QCOMPARE(report.payees[0].payee, QString("Landlord"));
        QCOMPARE(report.payees[1].payee, QString("REWE"));
        QCOMPARE(report.payees[1].spentCents, Cents(5500));
        QCOMPARE(report.payees[1].count, 4);
        QCOMPARE(report.payees[2].payee, QString());
        QCOMPARE(report.totalCents, Cents(89000 + 5500 + 700));

        QCOMPARE(Reports::topPayees(1, savings, month).totalCents, Cents(1000));
    }

    void otherUsersAreLeftOut() {
        db.seedDefaultUser(2, "other", "Other");
        const int theirs = AccountDAO::createAccount(2, "Theirs", "Checking", 0);
        add(theirs, 9900, QDate(2026, 10, 2), 0, "expense");
        QCOMPARE(Reports::spendingByCategory(1, -1, ReportRange::forPreset(Preset::Month, today), today).totalCents, Cents(0));
    }
};

QTEST_MAIN(TestReportData)
#include "test_reportdata.moc"
