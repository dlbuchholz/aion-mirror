#include <QtTest>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/backup.h"
#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "db/migrationmanager.h"
#include "settings.h"

class TestBackupBeforeUpdate : public QObject {
    Q_OBJECT

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;
    int databaseCounter = 0;
    QString path;

    // Makes the open database look like it's missing the newest migration, or has one from the future
    static void setAppliedVersion(int version) {
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("DELETE FROM schema_migrations"));
        query.prepare("INSERT INTO schema_migrations (version) VALUES (:version)");
        query.bindValue(":version", version);
        QVERIFY(query.exec());
    }

    QStringList backups() const {
        return QDir(Backup::folderFor(path)).entryList({"*.db"}, QDir::Files);
    }

    // What the file says, reading it without going through the app's connection
    static QList<int> versionsIn(const QString& file) {
        QList<int> versions;
        {
            QSqlDatabase copy = QSqlDatabase::addDatabase("QSQLITE", "versionCheck");
            copy.setDatabaseName(file);
            if (copy.open()) {
                QSqlQuery query(copy);
                query.exec("SELECT version FROM schema_migrations ORDER BY version");
                while (query.next()) {
                    versions << query.value(0).toInt();
                }
            }
            copy.close();
        }
        QSqlDatabase::removeDatabase("versionCheck");
        return versions;
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

    void init() {
        QDir(dir.filePath("backups")).removeRecursively();
        QFile::remove(dir.filePath("backups"));
        path = dir.filePath(QString("case%1.db").arg(++databaseCounter));
        QVERIFY(DatabaseSession::create(path));
    }

    void aNewDatabaseGetsNoBackup() {
        QVERIFY(backups().isEmpty());
    }

    void anUpToDateDatabaseGetsNoBackup() {
        QVERIFY(DatabaseSession::open(path));
        QVERIFY(backups().isEmpty());
    }

    void anOlderDatabaseIsBackedUpAsItWasThenUpdated() {
        setAppliedVersion(0);
        QVERIFY(DatabaseSession::open(path));

        const QStringList saved = backups();
        QCOMPARE(saved.size(), 1);
        QVERIFY(saved.first().contains("-before-update-"));
        QCOMPARE(versionsIn(QDir(Backup::folderFor(path)).filePath(saved.first())), QList<int>({0}));   // Before the update
        QVERIFY(versionsIn(path).contains(1));                                                          // After it

        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(backups().size(), 1);                                                                  // Done, so no more
    }

    void withoutABackupTheDatabaseStaysAsItWas() {
        setAppliedVersion(0);
        QFile blocker(dir.filePath("backups"));                   // A file where the folder should go
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        QString error;
        QVERIFY(!DatabaseSession::open(path, &error));
        QVERIFY(error.contains("backed up"));
        QCOMPARE(versionsIn(path), QList<int>({0}));
    }

    void keepsTheLastFive() {
        QVERIFY(QDir().mkpath(Backup::folderFor(path)));
        const QString base = QFileInfo(path).completeBaseName();
        for (int day = 1; day <= 6; ++day) {
            QFile old(QDir(Backup::folderFor(path)).filePath(QString("%1-before-update-2020-01-0%2-120000.db").arg(base).arg(day)));
            QVERIFY(old.open(QIODevice::WriteOnly));
        }
        QFile other(QDir(Backup::folderFor(path)).filePath("another-before-update-2020-01-01-120000.db"));
        QVERIFY(other.open(QIODevice::WriteOnly));            // Another database's stays
        other.close();

        QVERIFY(Backup::beforeUpdate());
        const QStringList saved = backups();
        QCOMPARE(saved.size(), 6);
        QVERIFY(saved.contains("another-before-update-2020-01-01-120000.db"));
        QVERIFY(!saved.contains(QString("%1-before-update-2020-01-02-120000.db").arg(base)));
        QVERIFY(saved.contains(QString("%1-before-update-2020-01-03-120000.db").arg(base)));
    }

    void aNewerDatabaseIsRefusedAndLeftAlone() {
        setAppliedVersion(9999);
        QString error;
        QVERIFY(!DatabaseSession::open(path, &error));
        QVERIFY(error.contains("newer version"));
        QVERIFY(backups().isEmpty());
        QCOMPARE(versionsIn(path), QList<int>({9999}));
    }

    void opensTheLastDatabaseAgainAfterARefusal() {
        const QString newer = dir.filePath("newer.db");
        QVERIFY(DatabaseSession::create(newer));
        setAppliedVersion(9999);
        QVERIFY(DatabaseSession::create(path + ".second"));
        QVERIFY(!DatabaseSession::open(newer));
        QCOMPARE(DatabaseSession::currentDatabasePath(), QDir::cleanPath(path + ".second"));
        QVERIFY(DatabaseManager::instance().database().isOpen());
    }

    void managerKnowsWhatItsLookingAt() {
        MigrationManager migrations(BuiltInMigrations);
        QVERIFY(!migrations.needsUpdate());
        QVERIFY(!migrations.isNewerThanApp());
        setAppliedVersion(0);
        QVERIFY(migrations.needsUpdate());
        setAppliedVersion(9999);
        QVERIFY(migrations.isNewerThanApp());
    }
};

QTEST_MAIN(TestBackupBeforeUpdate)
#include "test_backupbeforeupdate.moc"
