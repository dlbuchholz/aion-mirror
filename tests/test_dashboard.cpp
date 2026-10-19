#include <QtTest>
#include "account.h"
#include "dashboard.h"
#include "tests/test_db_fixture.h"

class TestDashboard : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;
    int savings = -1;
    int groceries = 0;
    int rent = 0;
    const QDate today = QDate(2026, 10, 18);

    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    int add(int account, Cents cents, const QDate& date, int category, const char* type, const char* payee = "Shop") {
        return TransactionDAO().addTransaction(Transaction(-1, account, payee, cents, date, "", category, type));
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

    void monthTotalsAndBalance() {
        add(giro, 285000, QDate(2026, 10, 15), categoryId("Salary"), "income");
        add(giro, 89000, QDate(2026, 10, 3), rent, "expense");
        add(savings, 50000, QDate(2026, 9, 2), 0, "income");
        add(giro, 3600, QDate(2026, 10, 18), groceries, "expense");

        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.totalBalanceCents, Cents(285000 - 89000 + 50000 - 3600));
        QCOMPARE(data.thisMonth.incomeCents, Cents(285000));
        QCOMPARE(data.thisMonth.incomeCount, 1);
        QCOMPARE(data.thisMonth.expenseCents, Cents(92600));
        QCOMPARE(data.thisMonth.netCents(), Cents(285000 - 92600));
        QCOMPARE(data.lastMonth.incomeCents, Cents(50000));
        QCOMPARE(data.dailyAverageCents, Cents(92600 / 18));
        QCOMPARE(data.daysLeftInMonth, 13);
    }

    void cashFlowCoversSixMonthsEndingNow() {
        add(giro, 1000, QDate(2026, 5, 1), 0, "expense");          // First shown month
        add(giro, 7000, QDate(2026, 4, 30), 0, "expense");         // Too early
        add(giro, 2000, QDate(2026, 10, 31), 0, "expense");        // Later this month still counts

        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.cashFlow.size(), 6);
        QCOMPARE(data.cashFlow.first().month, QDate(2026, 5, 1));
        QCOMPARE(data.cashFlow.first().expenseCents, Cents(1000));
        QCOMPARE(data.cashFlow.last().month, QDate(2026, 10, 1));
        QCOMPARE(data.cashFlow.last().expenseCents, Cents(2000));
        QCOMPARE(data.cashFlow[2].expenseCents, Cents(0));         // Empty months are there too
    }

    void lastMonthIsComparedUpToTheSameDay() {
        add(giro, 1000, QDate(2026, 9, 18), 0, "expense");
        add(giro, 9000, QDate(2026, 9, 19), 0, "expense");         // After the same day
        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.lastMonthToDateExpenseCents, Cents(1000));
        QCOMPARE(data.lastMonth.expenseCents, Cents(10000));

        // Short months: the 31st compares with all of the previous month
        QCOMPARE(Dashboard::load(1, QDate(2026, 10, 31)).lastMonthToDateExpenseCents, Cents(10000));
    }

    void spendingByCategoryIsSortedAndIncludesUncategorised() {
        add(giro, 31200, QDate(2026, 10, 1), groceries, "expense");
        add(savings, 4640, QDate(2026, 10, 2), groceries, "expense");
        add(giro, 89000, QDate(2026, 10, 3), rent, "expense");
        add(giro, 1250, QDate(2026, 10, 4), 0, "expense");
        add(giro, 9999, QDate(2026, 9, 30), rent, "expense");       // Last month
        add(giro, 5000, QDate(2026, 10, 5), groceries, "income");   // Refunds aren't spending

        const QList<CategorySpending> spending = Dashboard::load(1, today).spendingByCategory;
        QCOMPARE(spending.size(), 3);
        QCOMPARE(spending[0].categoryName, QString("Rent"));
        QCOMPARE(spending[1].spentCents, Cents(35840));
        QCOMPARE(spending[2].categoryId, 0);
        QCOMPARE(spending[2].categoryName, QString("No category"));
    }

    void recentTransactionsNewestFirstWithoutFutureOnes() {
        add(giro, 100, QDate(2026, 10, 1), 0, "expense", "First");
        add(giro, 200, QDate(2026, 10, 18), groceries, "expense", "Today");
        add(giro, 300, QDate(2026, 10, 25), 0, "expense", "Planned");
        add(giro, 400, QDate(2026, 10, 18), 0, "income", "Today later");
        for (int i = 0; i < 4; ++i) add(savings, 10, QDate(2026, 9, 1), 0, "expense", "Old");

        const QList<RecentTransaction> recent = Dashboard::load(1, today).recent;
        QCOMPARE(recent.size(), 5);
        QCOMPARE(recent[0].payee, QString("Today later"));          // Same day: entered last first
        QCOMPARE(recent[0].signedCents, Cents(400));
        QCOMPARE(recent[1].categoryName, QString("Groceries"));
        QCOMPARE(recent[1].signedCents, Cents(-200));
        QCOMPARE(recent[2].payee, QString("First"));
    }

    void startingBalancesArentIncomeOrSpending() {
        const int initial = categoryId("Initial balance");
        QVERIFY(initial > 0);
        add(giro, 250000, QDate(2026, 10, 1), initial, "income", "Start");
        add(savings, 40000, QDate(2026, 10, 2), initial, "expense", "Start overdrawn");
        add(giro, 3600, QDate(2026, 10, 3), groceries, "expense");

        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.totalBalanceCents, Cents(250000 - 40000 - 3600));
        QCOMPARE(data.thisMonth.incomeCents, Cents(0));
        QCOMPARE(data.thisMonth.incomeCount, 0);
        QCOMPARE(data.thisMonth.expenseCents, Cents(3600));
        QCOMPARE(data.thisMonth.expenseCount, 1);
        QCOMPARE(data.cashFlow.last().netCents(), Cents(-3600));
        QCOMPARE(data.dailyAverageCents, Cents(3600 / 18));
        QCOMPARE(data.spendingByCategory.size(), 1);
        QCOMPARE(data.spendingByCategory[0].categoryId, groceries);
        QCOMPARE(data.recent.size(), 1);                            // Not "recent transactions" either
        QCOMPARE(data.recent[0].categoryName, QString("Groceries"));

        add(giro, 7000, QDate(2026, 9, 5), initial, "expense");
        QCOMPARE(Dashboard::load(1, today).lastMonthToDateExpenseCents, Cents(0));
    }

    void otherUsersDontCount() {
        db.seedDefaultUser(2, "other", "Other");
        const int theirs = AccountDAO::createAccount(2, "Theirs", "Checking", 0);
        add(theirs, 5000, QDate(2026, 10, 2), groceries, "expense");
        const DashboardData data = Dashboard::load(1, today);
        QCOMPARE(data.thisMonth.expenseCents, Cents(0));
        QVERIFY(data.spendingByCategory.isEmpty());
        QVERIFY(data.recent.isEmpty());
        QCOMPARE(data.totalBalanceCents, Cents(0));
    }

    void greetingFollowsTheTimeOfDay() {
        QCOMPARE(Dashboard::greeting(QTime(4, 59)), QString("Good evening"));
        QCOMPARE(Dashboard::greeting(QTime(5, 0)), QString("Good morning"));
        QCOMPARE(Dashboard::greeting(QTime(11, 59)), QString("Good morning"));
        QCOMPARE(Dashboard::greeting(QTime(12, 0)), QString("Good afternoon"));
        QCOMPARE(Dashboard::greeting(QTime(18, 0)), QString("Good evening"));
    }
};

QTEST_MAIN(TestDashboard)
#include "test_dashboard.moc"
