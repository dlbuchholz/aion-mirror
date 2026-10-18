#include <QtTest>
#include "budget.h"
#include "tests/test_db_fixture.h"

class TestBudgetDAO : public QObject {
    Q_OBJECT
    TestDbFixture db;
private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void createAndReadForUser() {
        Budget budget(1, 1, "Groceries", 250.0, QDate(2026,1,1), QDate(2026,1,31));
        BudgetDAO dao;
        QVERIFY(dao.createBudget(budget));
        const auto budgets = dao.getAllBudgets(1);
        QCOMPARE(budgets.size(), 1);
        QCOMPARE(budgets.first().getUserId(), 1);
        QCOMPARE(budgets.first().getCategory(), QString("Groceries"));
        QCOMPARE(budgets.first().getAmount(), 250.0);
        QVERIFY(dao.deleteBudget(1));
        QVERIFY(!dao.deleteBudget(1));
    }

    void filtersByUserAndRejectsInvalidForeignKey() {
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("INSERT INTO users (id, username, name) VALUES (2, 'other', 'Other')"));
        BudgetDAO dao;
        QVERIFY(dao.createBudget(Budget(1, 1, "Rent", 100.0, QDate(2026,1,1), QDate(2026,1,31))));
        QVERIFY(dao.createBudget(Budget(2, 2, "Rent", 200.0, QDate(2026,1,1), QDate(2026,1,31))));
        QCOMPARE(dao.getAllBudgets(1).size(), 1);
        QCOMPARE(dao.getAllBudgets(2).size(), 1);
        QVERIFY(!dao.createBudget(Budget(3, 99, "Rent", 1.0, QDate(2026,1,1), QDate(2026,1,31))));
    }

    void spentAmountCountsOnlyMatchingExpensesInPeriod() {
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("INSERT INTO users (id, username, name) VALUES (2, 'other', 'Other')"));
        QVERIFY(q.exec("INSERT INTO accounts (id, user_id, name, type_id) VALUES (10, 1, 'Mine', 1), (20, 2, 'Theirs', 1)"));
        QVERIFY(q.exec("INSERT INTO transactions (account_id, amount, date, category_id, type) VALUES "
                       "(10, 40.0, '2026-01-05', (SELECT id FROM categories WHERE name = 'Groceries'), 'expense'),"
                       "(10, 10.5, '2026-01-31', (SELECT id FROM categories WHERE name = 'Groceries'), 'expense'),"
                       "(10, 99.0, '2026-02-01', (SELECT id FROM categories WHERE name = 'Groceries'), 'expense'),"
                       "(10, 77.0, '2026-01-10', (SELECT id FROM categories WHERE name = 'Rent'), 'expense'),"
                       "(10, 55.0, '2026-01-10', (SELECT id FROM categories WHERE name = 'Groceries'), 'income'),"
                       "(20, 33.0, '2026-01-10', (SELECT id FROM categories WHERE name = 'Groceries'), 'expense')"));

        BudgetDAO dao;
        QCOMPARE(dao.getSpentAmount(Budget(1, 1, "Groceries", 100.0, QDate(2026,1,1), QDate(2026,1,31))), 50.5);
        QCOMPARE(dao.getSpentAmount(Budget(2, 1, "Utilities", 100.0, QDate(2026,1,1), QDate(2026,1,31))), 0.0);
    }
};

QTEST_MAIN(TestBudgetDAO)
#include "test_budget.moc"
