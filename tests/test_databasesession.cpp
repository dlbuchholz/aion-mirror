#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"

class TestDatabaseSession : public QObject {
    Q_OBJECT

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;

    QString path(const QString& name) const { return dir.filePath(name); }

    static int userCount() {
        QSqlQuery query(DatabaseManager::instance().database());
        return query.exec("SELECT COUNT(*) FROM users") && query.next() ? query.value(0).toInt() : -1;
    }

private slots:
    void initTestCase() {
        QVERIFY(dir.isValid());
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }

    void cleanupTestCase() {
        DatabaseManager::instance().closeDatabase();
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }

    void createMigratesAndRemembersTheFile() {
        QString error;
        QVERIFY2(DatabaseSession::create(path("first.db"), &error), qPrintable(error));
        QCOMPARE(DatabaseSession::currentDatabasePath(), QDir::cleanPath(path("first.db")));
        QCOMPARE(userCount(), 0);                    // Schema exists
        QCOMPARE(Settings::instance().getLastDatabase(), QDir::cleanPath(path("first.db")));
        QCOMPARE(Settings::instance().getRecentDatabases().first(), QDir::cleanPath(path("first.db")));
    }

    void createRefusesExistingFiles() {
        QString error;
        QVERIFY(!DatabaseSession::create(path("first.db"), &error));
        QVERIFY(error.contains("already exists"));
    }

    void openSwitchesBetweenDatabases() {
        QVERIFY(DatabaseSession::create(path("second.db")));
        QSqlQuery insert(DatabaseManager::instance().database());
        QVERIFY(insert.exec("INSERT INTO users (username, name) VALUES ('sam', 'Sam')"));
        QCOMPARE(userCount(), 1);

        QVERIFY(DatabaseSession::open(path("first.db")));
        QCOMPARE(userCount(), 0);
        QVERIFY(DatabaseSession::open(path("second.db")));
        QCOMPARE(userCount(), 1);
    }

    void rejectsFilesThatAreNotAionDatabases() {
        // Plain text file
        QFile text(path("notes.db"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("just some text, definitely not SQLite");
        text.close();

        QString error;
        QVERIFY(!DatabaseSession::open(path("notes.db"), &error));
        QVERIFY(!error.isEmpty());
        // The previous database is still open and usable
        QCOMPARE(DatabaseSession::currentDatabasePath(), QDir::cleanPath(path("second.db")));
        QCOMPARE(userCount(), 1);

        // SQLite file of another program
        {
            QSqlDatabase foreign = QSqlDatabase::addDatabase("QSQLITE", "foreign");
            foreign.setDatabaseName(path("other.sqlite"));
            QVERIFY(foreign.open());
            QSqlQuery query(foreign);
            QVERIFY(query.exec("CREATE TABLE notes (id INTEGER PRIMARY KEY, text TEXT)"));
            foreign.close();
        }
        QSqlDatabase::removeDatabase("foreign");
        QVERIFY(!DatabaseSession::open(path("other.sqlite"), &error));
        QVERIFY(error.contains("another program"));
        QCOMPARE(userCount(), 1);

        // Missing file
        QVERIFY(!DatabaseSession::open(path("missing.db"), &error));
        QCOMPARE(userCount(), 1);
    }
};

QTEST_MAIN(TestDatabaseSession)
#include "test_databasesession.moc"
