#include <QtTest>
#include "user.h"
#include "category.h"
#include "account.h"
#include "balance.h"
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

    void anUnknownUserComesBackInvalid() {
        const User nobody = UserDAO().getUserById(9999);
        QCOMPARE(nobody.getUserId(), -1);
        QVERIFY(nobody.getName().isEmpty());
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
        CategoryDAO dao(1);
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
        CategoryDAO dao(1);
        const int travel = dao.addCategory(Category(-1, "Travel", "expense"));
        addExpense(travel, "A");
        addExpense(travel, "B");
        addBudget(travel, "2026-10");
        const QHash<int, CategoryUsage> usage = dao.usage();
        QCOMPARE(usage.value(travel).transactions, 2);
        QCOMPARE(usage.value(travel).budgets, 1);
    }

    void mergingMovesTransactionsAndBudgets() {
        CategoryDAO dao(1);
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
        CategoryDAO dao(1);
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        const int into = dao.addCategory(Category(-1, "Food", "expense"));
        addBudget(from, "2026-10");
        addBudget(into, "2026-01");
        QVERIFY(dao.mergeCategory(from, into));
        QCOMPARE(budgetCount(into), 1);
        QCOMPARE(budgetCount(from), 0);
    }

    void deletingCanKeepTheTransactions() {
        CategoryDAO dao(1);
        const int travel = dao.addCategory(Category(-1, "Travel", "expense"));
        const int transaction = addExpense(travel, "Train");
        addBudget(travel, "2026-10");
        QVERIFY(!dao.deleteCategory(travel));                    // Still used
        QVERIFY(dao.deleteCategoryKeepingTransactions(travel));
        QVERIFY(categoryOf(transaction).isNull());
        QCOMPARE(budgetCount(travel), 0);
    }

    void newUsersStartWithTheirOwnDefaults() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        const QList<Category> mine = CategoryDAO(1).getAllCategories();
        const QList<Category> theirs = CategoryDAO(2).getAllCategories();
        QCOMPARE(theirs.size(), mine.size());
        QVERIFY(std::any_of(theirs.cbegin(), theirs.cend(), [](const Category& c) { return c.getName() == BalanceCorrectionCategory; }));
        for (const Category& category : theirs) {
            QVERIFY(std::none_of(mine.cbegin(), mine.cend(), [&](const Category& c) { return c.getCategoryId() == category.getCategoryId(); }));
        }
        QCOMPARE(AccountDAO().getAccountTypes(2), AccountDAO().getAccountTypes(1));
        QVERIFY(AccountDAO().getAllAccountTypes(2).first().first != AccountDAO().getAllAccountTypes(1).first().first);
    }

    void defaultCategoriesHaveTheirOwnColours() {
        QHash<QString, QString> colours;
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            colours.insert(category.getName(), category.getColor());
        }
        QCOMPARE(colours.value("Groceries"), QString("amber"));
        QCOMPARE(colours.value("Rent"), QString("blue"));
        QCOMPARE(colours.value("Utilities"), QString("coral"));
        QCOMPARE(colours.value("Entertainment"), QString("purple"));
        QCOMPARE(colours.value("Salary"), QString("teal"));
        QCOMPARE(colours.value("Bonus"), QString("green"));
        QCOMPARE(colours.value(InitialBalanceCategory), QString("gray"));
        QCOMPARE(colours.value(BalanceCorrectionCategory), QString("gray"));
        QCOMPARE(colours.value(TransferCategory), QString("slate"));
    }

    void transferIsBuiltInButNotABalanceAdjustment() {
        // Locked like the balance adjustments, but set by hand, rules and imports like any category
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == TransferCategory) {
                QVERIFY(category.isBuiltIn());
                QVERIFY(!category.isBalanceAdjustment());
                QCOMPARE(category.getType(), QString("income"));   // Never offered for a budget
                return;
            }
        }
        QFAIL("New users have no Transfer category");
    }

    void usersDontSeeOrChangeEachOthersCategories() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        CategoryDAO mine(1);
        CategoryDAO theirs(2);
        const int travel = mine.addCategory(Category(-1, "Travel", "expense"));
        QVERIFY(theirs.addCategory(Category(-1, "Travel", "expense")) > 0);   // Same name, other user
        QVERIFY(mine.addCategory(Category(-1, "Travel", "expense")) <= 0);    // Not twice for one user

        const int theirTrips = theirs.addCategory(Category(-1, "Trips", "expense"));
        QCOMPARE(mine.getCategoryById(theirTrips).getCategoryId(), -1);
        QVERIFY(!mine.editCategory(theirTrips, "Mine now", "expense"));
        QVERIFY(!mine.mergeCategory(theirTrips, travel));
        QVERIFY(!mine.deleteCategoryKeepingTransactions(theirTrips));
        QVERIFY(!mine.deleteCategory(theirTrips));
        QCOMPARE(theirs.getCategoryById(theirTrips).getName(), QString("Trips"));
    }

    void accountTypesBelongToTheirUser() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        AccountDAO dao;
        const int depot = dao.addAccountType(2, "Depot");
        QVERIFY(depot > 0);
        QVERIFY(!dao.getAccountTypes(1).contains("Depot"));
        QVERIFY(!dao.editAccountType(1, depot, "Taken"));
        QVERIFY(!dao.deleteAccountType(1, depot));
        QVERIFY(AccountDAO::createAccount(1, "Wertpapiere", "Depot", 0) <= 0);   // Not one of user 1's types
        QVERIFY(AccountDAO::createAccount(2, "Wertpapiere", "Depot", 0) > 0);
    }

    void startingBalancesAndCorrectionsUseTheAccountUsersCategories() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        const int account = AccountDAO::createAccount(2, "Giro", "Checking", 5000);
        QVERIFY(Balance::addCorrection(account, QDate(2026, 10, 1), -700, "") > 0);
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT c.user_id, c.name FROM transactions t JOIN categories c ON c.id = t.category_id "
                      "WHERE t.account_id = :account ORDER BY t.id");
        query.bindValue(":account", account);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 2);
        QCOMPARE(query.value(1).toString(), InitialBalanceCategory);
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 2);
        QCOMPARE(query.value(1).toString(), BalanceCorrectionCategory);
    }

    void categoryCrudAndValidation() {
        CategoryDAO dao(1);
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
