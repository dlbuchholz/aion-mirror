#include <QtTest>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"

class TestStartupDatabase : public QObject {
    Q_OBJECT

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;
    int databaseCounter = 0;

    QString newDatabase() {
        const QString path = QDir::cleanPath(dir.filePath(QString("case%1.db").arg(++databaseCounter)));
        DatabaseManager::instance().closeDatabase();
        if (!DatabaseSession::create(path)) {
            return QString();
        }
        return path;
    }

private slots:
    void initTestCase() {
        QVERIFY(dir.isValid());
        QStandardPaths::setTestModeEnabled(true);             // The default database mustn't be the real one
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }

    void cleanupTestCase() {
        DatabaseManager::instance().closeDatabase();
        QFile::remove(DatabaseSession::defaultDatabasePath());
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }

    void opensTheLastDatabaseWithoutAWarning() {
        const QString last = newDatabase();
        QVERIFY(!last.isEmpty());
        QString warning = "untouched";
        QVERIFY(DatabaseSession::openAtStartup(&warning));
        QCOMPARE(DatabaseSession::currentDatabasePath(), last);
        QCOMPARE(warning, QString("untouched"));
    }

    void opensTheDefaultWithoutAWarningOnAFirstStart() {
        Settings::instance().setLastDatabase("");
        QString warning;
        QVERIFY(DatabaseSession::openAtStartup(&warning));
        QCOMPARE(DatabaseSession::currentDatabasePath(), DatabaseSession::defaultDatabasePath());
        QVERIFY(warning.isEmpty());
    }

    void warnsWhenTheLastDatabaseIsGone() {
        Settings::instance().setLastDatabase(dir.filePath("gone.db"));
        QString warning;
        QVERIFY(DatabaseSession::openAtStartup(&warning));
        QCOMPARE(DatabaseSession::currentDatabasePath(), DatabaseSession::defaultDatabasePath());
        QVERIFY(warning.contains("gone.db"));
        QVERIFY(warning.contains("default database"));
    }

    void warnsWhenTheLastDatabaseIsFromANewerVersion() {
        const QString newer = newDatabase();
        QVERIFY(!newer.isEmpty());
        {
            QSqlQuery query(DatabaseManager::instance().database());
            QVERIFY(query.exec("INSERT INTO schema_migrations (version) VALUES (9999)"));
        }
        DatabaseManager::instance().closeDatabase();

        QString warning;
        QVERIFY(DatabaseSession::openAtStartup(&warning));
        QCOMPARE(DatabaseSession::currentDatabasePath(), DatabaseSession::defaultDatabasePath());
        QVERIFY(warning.contains(QFileInfo(newer).fileName()));
        QVERIFY(warning.contains("newer version"));
    }
};

QTEST_MAIN(TestStartupDatabase)
#include "test_startupdatabase.moc"
