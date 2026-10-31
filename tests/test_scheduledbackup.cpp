#include <QtTest>
#include <QComboBox>
#include <QSpinBox>
#include <QTemporaryDir>

#include "db/backup.h"
#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "user.h"
#include "widgets/backupsettings.h"

class TestScheduledBackup : public QObject {
    Q_OBJECT

    using Schedule = Settings::BackupSchedule;

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;
    int caseCounter = 0;
    QString path;
    QString folder;

    QList<Backup::AutomaticBackup> scheduledBackups() const {
        QList<Backup::AutomaticBackup> found;
        for (const Backup::AutomaticBackup& backup : Backup::automaticBackups(path)) {
            if (backup.kind == "Scheduled") {
                found.append(backup);
            }
        }
        return found;
    }

    // A scheduled backup that looks this many days old, with the database not changed since
    void addOldBackup(int daysAgo) {
        QVERIFY(QDir().mkpath(Backup::folderFor(path)));
        const QDateTime when = QDateTime::currentDateTime().addDays(-daysAgo);
        QFile file(QDir(Backup::folderFor(path)).filePath(
            QString("budget-scheduled-%1.db").arg(when.toString("yyyy-MM-dd-HHmmss"))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        setDatabaseModified(when.addSecs(-60));
    }

    void setDatabaseModified(const QDateTime& when) {
        DatabaseManager::instance().closeDatabase();
        QFile database(path);
        QVERIFY(database.open(QIODevice::ReadWrite));
        QVERIFY(database.setFileTime(when, QFileDevice::FileModificationTime));
        database.close();
        QVERIFY(DatabaseManager::instance().openDatabase(path));
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
        Settings::instance().getQSettings().remove("backup");
        folder = dir.filePath(QString("case%1").arg(++caseCounter));
        QVERIFY(QDir().mkpath(folder));
        path = QDir::cleanPath(QDir(folder).filePath("budget.db"));
        QVERIFY(DatabaseSession::create(path));
        QVERIFY(UserDAO().createUser(User(1, "testuser", "Test User")));
    }

    void defaultsToEveryDayKeepingSeven() {
        QCOMPARE(Settings::instance().getBackupSchedule(), Schedule::Daily);
        QCOMPARE(Settings::instance().getScheduledBackupsToKeep(), 7);
    }

    void isDueFollowsTheSchedule() {
        const QDate today(2026, 3, 31);
        const auto daysAgo = [&](int days) { return QDateTime(today.addDays(-days), QTime(12, 0)); };

        QVERIFY(!Backup::isDue(Schedule::Off, QDateTime(), today));
        QVERIFY(Backup::isDue(Schedule::Daily, QDateTime(), today));
        QVERIFY(Backup::isDue(Schedule::Monthly, QDateTime(), today));

        QVERIFY(!Backup::isDue(Schedule::Daily, daysAgo(0), today));
        QVERIFY(Backup::isDue(Schedule::Daily, daysAgo(1), today));
        QVERIFY(!Backup::isDue(Schedule::Weekly, daysAgo(6), today));
        QVERIFY(Backup::isDue(Schedule::Weekly, daysAgo(7), today));
        QVERIFY(!Backup::isDue(Schedule::Monthly, QDateTime(QDate(2026, 3, 1), QTime(12, 0)), today));
        QVERIFY(Backup::isDue(Schedule::Monthly, QDateTime(QDate(2026, 2, 28), QTime(12, 0)), today));
        QVERIFY(!Backup::isDue(Schedule::Off, daysAgo(400), today));
    }

    void openingBacksUpTheFirstTimeThenNotAgainThatDay() {
        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(scheduledBackups().size(), 1);
        QVERIFY(Backup::inspect(scheduledBackups().first().path).has_value());

        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(scheduledBackups().size(), 1);
    }

    void offMeansNoBackups() {
        Settings::instance().setBackupSchedule(Schedule::Off);
        QVERIFY(DatabaseSession::open(path));
        QVERIFY(scheduledBackups().isEmpty());
    }

    void aDatabaseWithoutUsersGetsNone() {
        const QString empty = QDir::cleanPath(QDir(folder).filePath("empty.db"));
        QVERIFY(DatabaseSession::create(empty));
        QVERIFY(DatabaseSession::open(empty));
        QVERIFY(Backup::automaticBackups(empty).isEmpty());
    }

    void unchangedSinceTheLastOneIsSkipped() {
        addOldBackup(3);
        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(scheduledBackups().size(), 1);
    }

    void changedSinceTheLastOneIsBackedUp() {
        addOldBackup(3);
        setDatabaseModified(QDateTime::currentDateTime().addDays(-1));
        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(scheduledBackups().size(), 2);
    }

    void notDueYetIsSkippedEvenWhenChanged() {
        Settings::instance().setBackupSchedule(Schedule::Weekly);
        addOldBackup(3);
        setDatabaseModified(QDateTime::currentDateTime().addDays(-1));
        QVERIFY(DatabaseSession::open(path));
        QCOMPARE(scheduledBackups().size(), 1);
    }

    void keepsAsManyAsSetAndLeavesOtherKindsAlone() {
        Settings::instance().setScheduledBackupsToKeep(2);
        QVERIFY(QDir().mkpath(Backup::folderFor(path)));
        const QDir backups(Backup::folderFor(path));
        for (const char* name : {"budget-scheduled-2020-01-01-120000.db", "budget-scheduled-2020-01-02-120000.db",
                                 "budget-scheduled-2020-01-03-120000.db", "budget-before-update-2020-01-01-120000.db"}) {
            QFile file(backups.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(DatabaseSession::open(path));

        QCOMPARE(scheduledBackups().size(), 2);
        QVERIFY(backups.exists("budget-scheduled-2020-01-03-120000.db"));
        QVERIFY(!backups.exists("budget-scheduled-2020-01-02-120000.db"));
        QVERIFY(backups.exists("budget-before-update-2020-01-01-120000.db"));
    }

    void aFailedBackupStillOpensTheDatabase() {
        QFile blocker(Backup::folderFor(path));                // A file where the folder should go
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QVERIFY(DatabaseSession::open(path));
        QVERIFY(DatabaseManager::instance().database().isOpen());
    }

    void theSectionShowsAndSavesTheChoices() {
        Settings::instance().setBackupSchedule(Schedule::Weekly);
        Settings::instance().setScheduledBackupsToKeep(4);
        BackupSettings settings;
        auto* combo = settings.findChild<QComboBox*>("backupScheduleCombo");
        auto* keep = settings.findChild<QSpinBox*>("backupKeepSpin");
        QCOMPARE(combo->currentText(), QString("Every week"));
        QCOMPARE(keep->value(), 4);

        combo->setCurrentIndex(combo->findText("Off"));
        QCOMPARE(Settings::instance().getBackupSchedule(), Schedule::Off);
        QVERIFY(!keep->isEnabled());

        combo->setCurrentIndex(combo->findText("Every month"));
        QCOMPARE(Settings::instance().getBackupSchedule(), Schedule::Monthly);
        QVERIFY(keep->isEnabled());

        keep->setValue(12);
        QCOMPARE(Settings::instance().getScheduledBackupsToKeep(), 12);
    }
};

QTEST_MAIN(TestScheduledBackup)
#include "test_scheduledbackup.moc"
