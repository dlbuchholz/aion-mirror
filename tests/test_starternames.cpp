#include <QtTest>

#include "account.h"
#include "category.h"
#include "db/databasesession.h"
#include "starternames.h"
#include "tests/fake_translator.h"
#include "tests/test_db_fixture.h"

// The account types and categories a user starts with follow the language until renamed
class TestStarterNames : public QObject {
    Q_OBJECT

    TestDbFixture db;

    static QHash<QString, QString> german() {
        return {{"Account/Checking", "Girokonto"}, {"Account/Savings", "Sparkonto"},
                {"Category/Groceries", "Lebensmittel"}, {"Category/Rent", "Miete"},
                {"Category/Salary", "Gehalt"}};
    }

    static QStringList accountTypes(int userId = 1) { return AccountDAO().getAccountTypes(userId); }

    static QStringList categories(int userId = 1) {
        QStringList names;
        for (const Category& category : CategoryDAO(userId).getAllCategories()) names << category.getName();
        return names;
    }

    static int categoryId(const QString& name, int userId = 1) {
        for (const Category& category : CategoryDAO(userId).getAllCategories()) {
            if (category.getName() == name) return category.getCategoryId();
        }
        return -1;
    }

    static int accountTypeId(const QString& name, int userId = 1) {
        for (const auto& type : AccountDAO().getAllAccountTypes(userId)) {
            if (type.second == name) return type.first.toInt();
        }
        return -1;
    }

    static int changes() {
        QSqlQuery query(DatabaseManager::instance().database());
        return query.exec("SELECT total_changes()") && query.next() ? query.value(0).toInt() : -1;
    }

    static QStringList column(const QString& sql) {
        QStringList values;
        QSqlQuery query(DatabaseManager::instance().database());
        if (query.exec(sql)) {
            while (query.next()) values << query.value(0).toString();
        }
        return values;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); }
    void cleanup() { db.close(); }

    void aNewUsersStartersAreInTheLanguage() {
        FakeTranslator translator(german());
        db.seedDefaultUser();
        QVERIFY(accountTypes().contains("Girokonto"));
        QVERIFY(categories().contains("Lebensmittel"));
        QVERIFY(!categories().contains("Groceries"));
    }

    void theyFollowWhenTheLanguageChanges() {
        {
            FakeTranslator translator(german());
            db.seedDefaultUser();
        }
        QVERIFY(StarterNames::followLanguage());                 // Now English
        QVERIFY(accountTypes().contains("Checking"));
        QVERIFY(!accountTypes().contains("Girokonto"));
        QVERIFY(categories().contains("Groceries"));
        QVERIFY(categories().contains("Salary"));

        FakeTranslator translator(german());                     // And back
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(accountTypes().contains("Sparkonto"));
        QVERIFY(categories().contains("Miete"));
        QVERIFY(categories().contains("Entertainment"));         // Not translated: as written
    }

    // They keep their English name in the data, where queries and checks find them
    void theBuiltInCategoriesAreNoStarters() {
        FakeTranslator translator(german());
        db.seedDefaultUser();
        QCOMPARE(column("SELECT name FROM categories WHERE starter IS NULL ORDER BY name"),
                 (QStringList{BalanceCorrectionCategory, InitialBalanceCategory, TransferCategory}));
        QCOMPARE(column("SELECT starter FROM categories WHERE name = 'Lebensmittel'"), QStringList{"Groceries"});
        QCOMPARE(column("SELECT starter FROM account_types WHERE name = 'Girokonto'"), QStringList{"Checking"});
    }

    void aRenamedOneIsTheUsersOwn() {
        db.seedDefaultUser();
        QVERIFY(AccountDAO().editAccountType(1, accountTypeId("Checking"), "Current account"));
        QVERIFY(CategoryDAO(1).editCategory(categoryId("Groceries"), "Supermarket", "expense"));
        QVERIFY(CategoryDAO(1).editCategory(categoryId("Rent"), "Flat", "expense", "blue"));

        FakeTranslator translator(german());
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(accountTypes().contains("Current account"));
        QVERIFY(accountTypes().contains("Sparkonto"));
        QVERIFY(categories().contains("Supermarket"));
        QVERIFY(categories().contains("Flat"));
        QVERIFY(categories().contains("Gehalt"));
    }

    // Saving it with another colour or type leaves the name, so it still follows
    void savingItUnderTheSameNameKeepsItFollowing() {
        db.seedDefaultUser();
        QVERIFY(AccountDAO().editAccountType(1, accountTypeId("Checking"), "Checking"));
        QVERIFY(CategoryDAO(1).editCategory(categoryId("Groceries"), "Groceries", "income"));
        QVERIFY(CategoryDAO(1).editCategory(categoryId("Rent"), "Rent", "expense", "teal"));

        FakeTranslator translator(german());
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(accountTypes().contains("Girokonto"));
        QVERIFY(categories().contains("Lebensmittel"));
        QVERIFY(categories().contains("Miete"));
    }

    // The user's own "Miete" stays, and the starter keeps its name until the way is free
    void aNameTheUserGaveAnotherStaysTheirs() {
        db.seedDefaultUser();
        QVERIFY(CategoryDAO(1).addCategory(Category(-1, "Miete", "expense")) > 0);
        QVERIFY(AccountDAO().addAccountType(1, "Girokonto") > 0);

        FakeTranslator translator(german());
        QVERIFY(StarterNames::followLanguage());
        QCOMPARE(categories().count("Miete"), 1);
        QVERIFY(categories().contains("Rent"));
        QVERIFY(categories().contains("Lebensmittel"));
        QVERIFY(accountTypes().contains("Checking"));
        QVERIFY(accountTypes().contains("Sparkonto"));

        QVERIFY(CategoryDAO(1).deleteCategory(categoryId("Miete")));
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(categories().contains("Miete"));
        QVERIFY(!categories().contains("Rent"));
    }

    void everyUsersStartersFollow() {
        db.seedDefaultUser(1, "first", "First");
        db.seedDefaultUser(2, "second", "Second");
        FakeTranslator translator(german());
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(categories(1).contains("Miete"));
        QVERIFY(categories(2).contains("Miete"));
        QVERIFY(accountTypes(2).contains("Girokonto"));
    }

    // Opening a database doesn't count as a change, which scheduled backups go by
    void whenNothingChangesNothingIsWritten() {
        db.seedDefaultUser();
        const int before = changes();
        QVERIFY(StarterNames::followLanguage());
        QCOMPARE(changes(), before);
    }

    void aDatabaseItCantReadIsReported() {
        db.seedDefaultUser();
        QVERIFY(TestDbFixture::dropTable("account_types"));
        FakeTranslator translator(german());
        QVERIFY(!StarterNames::followLanguage());
        QVERIFY(categories().contains("Miete"));                 // The categories follow all the same
    }

    void aDatabaseThatRefusesTheNewNameIsReported() {
        db.seedDefaultUser();
        QVERIFY(TestDbFixture::refuse("UPDATE", "categories"));
        FakeTranslator translator(german());
        QVERIFY(!StarterNames::followLanguage());
        QVERIFY(accountTypes().contains("Girokonto"));
    }

    // From before translations: the starters are English, as they were created
    void anOlderDatabasesStartersFollowAfterTheUpdate() {
        TestFolder folder;
        QVERIFY(TestDbFixture::createAtVersion(folder.filePath("older.db"), 1));
        QSqlQuery query(DatabaseManager::instance().database());
        // A user of Aion 0.1 who renamed Rent and added Car and a type of their own
        for (const char* sql : {
                 "INSERT INTO users (id, username, name) VALUES (1, 'anna', 'Anna')",
                 "INSERT INTO account_types (user_id, name) VALUES (1, 'Checking'), (1, 'Savings'), (1, 'Investment'), "
                 "(1, 'Retirement'), (1, 'Virtual'), (1, 'Credit card')",
                 "INSERT INTO categories (user_id, name, type) VALUES (1, 'Initial balance', 'income'), (1, 'Salary', 'income'), "
                 "(1, 'Bonus', 'income'), (1, 'Groceries', 'expense'), (1, 'Flat', 'expense'), (1, 'Utilities', 'expense'), "
                 "(1, 'Entertainment', 'expense'), (1, 'Balance correction', 'income'), (1, 'Transfer', 'income'), "
                 "(1, 'Car', 'expense')"}) {
            QVERIFY2(query.exec(sql), qPrintable(query.lastError().text()));
        }

        QVERIFY(MigrationManager(BuiltInMigrations).applyMigrations());
        QCOMPARE(column("SELECT starter FROM account_types WHERE starter IS NOT NULL ORDER BY id"),
                 (QStringList{"Checking", "Savings", "Investment", "Retirement", "Virtual"}));
        QCOMPARE(column("SELECT starter FROM categories WHERE starter IS NOT NULL ORDER BY id"),
                 (QStringList{"Salary", "Bonus", "Groceries", "Utilities", "Entertainment"}));

        FakeTranslator translator(german());
        QVERIFY(StarterNames::followLanguage());
        QVERIFY(categories().contains("Lebensmittel"));
        QVERIFY(categories().contains("Flat"));
        QVERIFY(categories().contains("Car"));
    }

    void openingADatabaseFollowsTheLanguage() {
        TestFolder folder;
        const QString path = folder.filePath("starters.db");
        QVERIFY(DatabaseSession::create(path));
        QVERIFY(UserDAO().createUser(User(1, "user", "User")));
        FakeTranslator translator(german());
        QVERIFY(DatabaseSession::open(path));
        QVERIFY(categories().contains("Miete"));
    }
};

QTEST_MAIN(TestStarterNames)
#include "test_starternames.moc"
