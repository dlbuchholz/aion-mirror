#include <QtTest>

#include "tests/test_db_fixture.h"

class TestIntegration: public QObject {
    Q_OBJECT

private:
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void testForeignKeysAreEnabled() {
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("PRAGMA foreign_keys"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void testForeignKeysRemainEnabledAfterReopen() {
        db.close();
        db.yankDb("reopened.db");
        QSqlQuery q(DatabaseManager::instance().database());
        QVERIFY(q.exec("PRAGMA foreign_keys"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

};

QTEST_MAIN(TestIntegration)
#include "test_integration.moc"
