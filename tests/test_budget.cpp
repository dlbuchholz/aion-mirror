#include <QtTest>
#include "account.h"
#include "budget.h"
#include "category.h"
#include "tests/test_db_fixture.h"

class TestBudgetDAO : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int accountId = -1;
    int groceries = 0;
    int rent = 0;
    int gifts = 0;

    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    void spend(Cents cents, const QDate& date, int category) {
        Transaction transaction(-1, accountId, "Shop", cents, date, "", category, "expense");
        QVERIFY(TransactionDAO().addTransaction(transaction) > 0);
    }

    static QMap<QString, Cents> limits(int userId, const QDate& month) {
        QMap<QString, Cents> result;
        for (const MonthBudget& budget : BudgetDAO().budgetsForMonth(userId, month)) {
            result.insert(budget.categoryName, budget.limitCents);
        }
        return result;
    }

    static const QDate sep() { return QDate(2026, 9, 1); }
    static const QDate oct() { return QDate(2026, 10, 15); }   // Any day of the month works
    static const QDate nov() { return QDate(2026, 11, 1); }
    static const QDate dec() { return QDate(2026, 12, 1); }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        accountId = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        groceries = categoryId("Groceries");
        rent = categoryId("Rent");
        gifts = categoryId("Entertainment");
        QVERIFY(accountId > 0 && groceries > 0 && rent > 0 && gifts > 0);
    }
    void cleanup() { db.close(); }

    void monthlyLimitAppliesFromItsMonthOn() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        QVERIFY(limits(1, sep()).isEmpty());
        QCOMPARE(limits(1, oct()).value("Groceries"), Cents(40000));
        QCOMPARE(limits(1, QDate(2027, 6, 1)).value("Groceries"), Cents(40000));

        const MonthBudget budget = dao.budgetsForMonth(1, nov()).first();
        QVERIFY(budget.repeats);
        QCOMPARE(budget.validFrom, QDate(2026, 10, 1));
    }

    void changingTheMonthlyLimitKeepsEarlierMonths() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        QVERIFY(dao.setMonthlyLimit(1, groceries, dec(), 45000));
        QCOMPARE(limits(1, oct()).value("Groceries"), Cents(40000));
        QCOMPARE(limits(1, nov()).value("Groceries"), Cents(40000));
        QCOMPARE(limits(1, dec()).value("Groceries"), Cents(45000));

        // Setting it again in the same month just updates it
        QVERIFY(dao.setMonthlyLimit(1, groceries, dec(), 50000));
        QCOMPARE(limits(1, dec()).value("Groceries"), Cents(50000));
        QCOMPARE(limits(1, nov()).value("Groceries"), Cents(40000));
    }

    void singleMonthValueOverridesOnlyThatMonth() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, gifts, oct(), 5000));
        QVERIFY(dao.setLimitForMonth(1, gifts, dec(), 30000));
        QCOMPARE(limits(1, nov()).value("Entertainment"), Cents(5000));
        QCOMPARE(limits(1, dec()).value("Entertainment"), Cents(30000));
        QCOMPARE(limits(1, QDate(2027, 1, 1)).value("Entertainment"), Cents(5000));
        QVERIFY(!dao.budgetsForMonth(1, dec()).first().repeats);

        // A single month works without a regular limit too
        QVERIFY(dao.setLimitForMonth(1, rent, nov(), 90000));
        QVERIFY(!limits(1, oct()).contains("Rent"));
        QCOMPARE(limits(1, nov()).value("Rent"), Cents(90000));
        QVERIFY(!limits(1, dec()).contains("Rent"));

        // Setting the regular limit in a month replaces that month's own value
        QVERIFY(dao.setMonthlyLimit(1, gifts, dec(), 6000));
        QCOMPARE(limits(1, dec()).value("Entertainment"), Cents(6000));
        QVERIFY(dao.budgetsForMonth(1, dec()).first().repeats);
    }

    void removingOnlyThisMonth() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        QVERIFY(dao.removeLimit(1, groceries, nov(), false));
        QCOMPARE(limits(1, oct()).value("Groceries"), Cents(40000));
        QVERIFY(!limits(1, nov()).contains("Groceries"));
        QCOMPARE(limits(1, dec()).value("Groceries"), Cents(40000));

        // A month's own value is removed and the regular limit shows again... unless that is
        // what was switched off: removing again keeps the month empty
        QVERIFY(dao.removeLimit(1, groceries, nov(), false));
        QVERIFY(!limits(1, nov()).contains("Groceries"));

        // Without a regular limit, removing a single month leaves nothing behind
        QVERIFY(dao.setLimitForMonth(1, rent, nov(), 90000));
        QVERIFY(dao.removeLimit(1, rent, nov(), false));
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec(QString("SELECT COUNT(*) FROM budgets WHERE category_id = %1").arg(rent)) && query.next());
        QCOMPARE(query.value(0).toInt(), 0);
    }

    void removingFromThisMonthOn() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, sep(), 40000));
        QVERIFY(dao.setLimitForMonth(1, groceries, dec(), 60000));
        QVERIFY(dao.removeLimit(1, groceries, nov(), true));
        QCOMPARE(limits(1, sep()).value("Groceries"), Cents(40000));
        QCOMPARE(limits(1, oct()).value("Groceries"), Cents(40000));
        QVERIFY(!limits(1, nov()).contains("Groceries"));
        QVERIFY(!limits(1, dec()).contains("Groceries"));   // Later single months go too

        // Starting again later works
        QVERIFY(dao.setMonthlyLimit(1, groceries, QDate(2027, 2, 1), 30000));
        QVERIFY(!limits(1, QDate(2027, 1, 1)).contains("Groceries"));
        QCOMPARE(limits(1, QDate(2027, 2, 1)).value("Groceries"), Cents(30000));
    }

    void spendingCountsOnlyTheMonthsExpensesOfTheUser() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        spend(31200, QDate(2026, 10, 1), groceries);
        spend(4640, QDate(2026, 10, 31), groceries);
        spend(9900, QDate(2026, 11, 1), groceries);              // Next month
        Transaction refund(-1, accountId, "Shop", 5500, QDate(2026, 10, 10), "", groceries, "income");
        QVERIFY(TransactionDAO().addTransaction(refund) > 0);    // Income doesn't count

        db.seedDefaultUser(2, "other", "Other");
        const int otherAccount = AccountDAO::createAccount(2, "Theirs", "Checking", 0);
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, otherAccount, "X", 3300, QDate(2026, 10, 5), "", groceries, "expense")) > 0);

        const MonthBudget budget = dao.budgetsForMonth(1, oct()).first();
        QCOMPARE(budget.spentCents, Cents(35840));
        QCOMPARE(budget.leftCents(), Cents(4160));
    }

    void unbudgetedSpendingListsTheRest() {
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        spend(31200, QDate(2026, 10, 1), groceries);
        spend(89000, QDate(2026, 10, 3), rent);
        spend(4640, QDate(2026, 10, 4), 0);                       // No category
        spend(1000, QDate(2026, 9, 4), gifts);                    // Other month

        const QList<UnbudgetedSpending> rest = dao.unbudgetedSpending(1, oct());
        QCOMPARE(rest.size(), 2);
        QCOMPARE(rest[0].categoryName, QString("Rent"));         // Largest first
        QCOMPARE(rest[0].spentCents, Cents(89000));
        QCOMPARE(rest[1].categoryId, 0);
        QCOMPARE(rest[1].categoryName, QString("No category"));
    }

    void startingBalancesArentSpending() {
        const int initial = categoryId("Initial balance");
        QVERIFY(initial > 0);
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, initial, oct(), 1000));
        spend(50000, QDate(2026, 10, 1), initial);
        spend(4640, QDate(2026, 10, 4), 0);

        QCOMPARE(dao.budgetsForMonth(1, oct()).first().spentCents, Cents(0));
        QVERIFY(dao.removeLimit(1, initial, oct(), true));
        const QList<UnbudgetedSpending> rest = dao.unbudgetedSpending(1, oct());
        QCOMPARE(rest.size(), 1);
        QCOMPARE(rest[0].categoryId, 0);
        QCOMPARE(rest[0].spentCents, Cents(4640));
    }

    void budgetsBelongToTheirUser() {
        db.seedDefaultUser(2, "other", "Other");
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        QVERIFY(dao.setMonthlyLimit(2, groceries, oct(), 10000));
        QCOMPARE(limits(1, oct()).value("Groceries"), Cents(40000));
        QCOMPARE(limits(2, oct()).value("Groceries"), Cents(10000));
        QVERIFY(!dao.setMonthlyLimit(99, groceries, oct(), 100));  // Unknown user
        QVERIFY(!dao.setMonthlyLimit(1, groceries, oct(), 0));     // Limits are positive
    }

    void renamingACategoryKeepsItsBudget() {
        // Budgets used to reference categories by name, so a rename broke them
        BudgetDAO dao;
        QVERIFY(dao.setMonthlyLimit(1, groceries, oct(), 40000));
        QVERIFY(CategoryDAO().editCategory(groceries, "Food", "expense"));
        QCOMPARE(limits(1, oct()).value("Food"), Cents(40000));
    }

    void monthKeys() {
        QCOMPARE(BudgetDAO::monthKey(QDate(2026, 1, 31)), QString("2026-01"));
    }
};

QTEST_MAIN(TestBudgetDAO)
#include "test_budget.moc"
