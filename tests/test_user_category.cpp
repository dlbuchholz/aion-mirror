#include <QtTest>
#include "user.h"
#include "category.h"
#include "tests/test_db_fixture.h"

class TestUserCategoryDAO : public QObject {
    Q_OBJECT
    TestDbFixture db;
private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void autoGeneratesUserId() {
        UserDAO dao;
        QVERIFY(dao.createUser(User(0, "generated", "Generated User")));
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("SELECT id FROM users WHERE username='generated'"));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toInt() > 0);
    }

    void listsUsersByNameAndFindsByUsername() {
        UserDAO dao;   // seedDefaultUser() added "testuser" / "Test User"
        QVERIFY(dao.createUser(User(0, "anna", "anna Zed")));
        QVERIFY(dao.createUser(User(0, "bob", "Bob")));

        const QList<User> users = dao.getAllUsers();
        QCOMPARE(users.size(), 3);
        QCOMPARE(users[0].getName(), QString("anna Zed"));   // Sorted case-insensitively
        QCOMPARE(users[1].getName(), QString("Bob"));
        QCOMPARE(users[2].getName(), QString("Test User"));

        QCOMPARE(dao.findUserIdByUsername("bob"), users[1].getUserId());
        QCOMPARE(dao.findUserIdByUsername("nobody"), -1);
    }

    void rejectsDuplicateUserFields() {
        UserDAO dao;
        QVERIFY(!dao.createUser(User(0, "testuser", "Another Name")));
        QVERIFY(!dao.createUser(User(0, "another", "Test User")));
    }

    static int addExpense(int category, const char* payee) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("INSERT OR IGNORE INTO accounts (id, user_id, name, type_id) VALUES (1, 1, 'Giro', 1)");
        query.prepare("INSERT INTO transactions (account_id, payee, amount, date, category_id, type) "
                      "VALUES (1, :payee, 100, '2026-10-01', :category, 'expense')");
        query.bindValue(":payee", payee);
        query.bindValue(":category", category);
        return query.exec() ? query.lastInsertId().toInt() : -1;
    }

    static QVariant categoryOf(int transactionId) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT category_id FROM transactions WHERE id = :id");
        query.bindValue(":id", transactionId);
        return query.exec() && query.next() ? query.value(0) : QVariant();
    }

    static int budgetCount(int category) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT COUNT(*) FROM budgets WHERE category_id = :id");
        query.bindValue(":id", category);
        return query.exec() && query.next() ? query.value(0).toInt() : -1;
    }

    static void addBudget(int category, const char* month) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("INSERT INTO budgets (user_id, category_id, amount, start_month, repeats) VALUES (1, :c, 100, :m, 1)");
        query.bindValue(":c", category);
        query.bindValue(":m", month);
        QVERIFY(query.exec());
    }

    void coloursAreStored() {
        CategoryDAO dao;
        const int id = dao.addCategory(Category(-1, "Travel", "expense", "teal"));
        QCOMPARE(dao.getCategoryById(id).getColor(), QString("teal"));
        QVERIFY(dao.editCategory(id, "Travel", "expense", ""));
        QCOMPARE(dao.getCategoryById(id).getColor(), QString());
        QVERIFY(dao.getCategoryById(id).isBuiltIn() == false);
        bool builtInFound = false;
        for (const Category& category : dao.getAllCategories()) builtInFound |= category.isBuiltIn();
        QVERIFY(builtInFound);
    }

    void usageCountsTransactionsAndBudgets() {
        CategoryDAO dao;
        const int travel = dao.addCategory(Category(-1, "Travel", "expense"));
        addExpense(travel, "A");
        addExpense(travel, "B");
        addBudget(travel, "2026-10");
        const QHash<int, CategoryUsage> usage = dao.usage();
        QCOMPARE(usage.value(travel).transactions, 2);
        QCOMPARE(usage.value(travel).budgets, 1);
    }

    void mergingMovesTransactionsAndBudgets() {
        CategoryDAO dao;
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        const int into = dao.addCategory(Category(-1, "Food", "expense"));
        const int moved = addExpense(from, "Rewe");
        addBudget(from, "2026-10");
        QVERIFY(dao.mergeCategory(from, into));
        QCOMPARE(categoryOf(moved).toInt(), into);
        QCOMPARE(budgetCount(into), 1);                          // The target had none: the budget moved
        QCOMPARE(dao.getCategoryById(from).getCategoryId(), -1);
        QVERIFY(!dao.mergeCategory(into, into));
        QVERIFY(!dao.mergeCategory(from, into));                 // Gone
    }

    void mergingKeepsTheTargetsOwnBudgets() {
        CategoryDAO dao;
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        const int into = dao.addCategory(Category(-1, "Food", "expense"));
        addBudget(from, "2026-10");
        addBudget(into, "2026-01");
        QVERIFY(dao.mergeCategory(from, into));
        QCOMPARE(budgetCount(into), 1);
        QCOMPARE(budgetCount(from), 0);
    }

    void deletingCanKeepTheTransactions() {
        CategoryDAO dao;
        const int travel = dao.addCategory(Category(-1, "Travel", "expense"));
        const int transaction = addExpense(travel, "Train");
        addBudget(travel, "2026-10");
        QVERIFY(!dao.deleteCategory(travel));                    // Still used
        QVERIFY(dao.deleteCategoryKeepingTransactions(travel));
        QVERIFY(categoryOf(transaction).isNull());
        QCOMPARE(budgetCount(travel), 0);
    }

    void categoryCrudAndValidation() {
        CategoryDAO dao;
        const int id = dao.addCategory(Category(-1, "Travel", "expense"));
        QVERIFY(id > 0);
        QCOMPARE(dao.getCategoryById(id).getName(), QString("Travel"));
        QVERIFY(dao.editCategory(id, "Trips", "expense"));
        QCOMPARE(dao.getCategoryById(id).getName(), QString("Trips"));
        QVERIFY(!dao.editCategory(id, "Trips", "invalid"));
        QVERIFY(dao.deleteCategory(id));
        QCOMPARE(dao.getCategoryById(id).getCategoryId(), -1);
        QVERIFY(!dao.deleteCategory(id));
    }
};

QTEST_MAIN(TestUserCategoryDAO)
#include "test_user_category.moc"
