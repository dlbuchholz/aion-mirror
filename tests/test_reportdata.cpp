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

    void otherUsersAreLeftOut() {
        db.seedDefaultUser(2, "other", "Other");
        const int theirs = AccountDAO::createAccount(2, "Theirs", "Checking", 0);
        add(theirs, 9900, QDate(2026, 10, 2), 0, "expense");
        QCOMPARE(Reports::spendingByCategory(1, -1, ReportRange::forPreset(Preset::Month, today), today).totalCents, Cents(0));
    }
};

QTEST_MAIN(TestReportData)
#include "test_reportdata.moc"
