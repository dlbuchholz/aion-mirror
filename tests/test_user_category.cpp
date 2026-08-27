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

    void rejectsDuplicateUserFields() {
        UserDAO dao;
        QVERIFY(!dao.createUser(User(0, "testuser", "Another Name")));
        QVERIFY(!dao.createUser(User(0, "another", "Test User")));
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
