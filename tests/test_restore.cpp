#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "account.h"
#include "db/backup.h"
#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "user.h"
#include "widgets/backupsettings.h"

class TestRestore : public QObject {
    Q_OBJECT

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;
    int caseCounter = 0;
    QString path;
    QString folder;
    int giro = -1;

    static int accountCount() {
        return AccountDAO().getAllAccountsForUser(1).size();
    }

    // A backup holding the one account so far, with another added to the open database afterwards
    QString backupThenChange() {
        const QString backup = QDir(folder).filePath("saved.db");
        if (!Backup::write(backup)) {
            return QString();
        }
        AccountDAO::createAccount(1, "Later", "Savings", 100);
        return backup;
    }

    // A file with these statements run in it
    QString fileWith(const QString& name, const QStringList& statements) {
        const QString file = QDir(folder).filePath(name);
        {
            QSqlDatabase other = QSqlDatabase::addDatabase("QSQLITE", "fileWith");
            other.setDatabaseName(file);
            other.open();
            QSqlQuery query(other);
            for (const QString& statement : statements) {
                query.exec(statement);
            }
            other.close();
        }
        QSqlDatabase::removeDatabase("fileWith");
        return file;
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
        folder = dir.filePath(QString("case%1").arg(++caseCounter));
        QVERIFY(QDir().mkpath(folder));
        path = QDir::cleanPath(QDir(folder).filePath("budget.db"));
        QVERIFY(DatabaseSession::create(path));
        QVERIFY(UserDAO().createUser(User(1, "testuser", "Test User")));
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 5000);
    }

    void inspectCountsWhatsInside() {
        const QString backup = backupThenChange();
        const std::optional<Backup::Contents> contents = Backup::inspect(backup);
        QVERIFY(contents.has_value());
        QCOMPARE(contents->users, 1);
        QCOMPARE(contents->accounts, 1);
        QCOMPARE(contents->transactions, 1);
    }

    void inspectRefusesWhatCantBeRestored() {
        QString error;
        QVERIFY(!Backup::inspect(QDir(folder).filePath("missing.db"), &error));
        QVERIFY(!QFile::exists(QDir(folder).filePath("missing.db")));

        QFile text(QDir(folder).filePath("notes.db"));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("These are not the transactions you're looking for, and there are quite a few of them.");
        text.close();
        QVERIFY(!Backup::inspect(text.fileName(), &error));

        const QString foreign = fileWith("foreign.db", {"CREATE TABLE notes (id INTEGER)"});
        QVERIFY(!Backup::inspect(foreign, &error));
        QVERIFY(error.contains("isn't an Aion"));

        const QString newer = fileWith("newer.db", {"CREATE TABLE schema_migrations (version INTEGER)",
                                                    "INSERT INTO schema_migrations VALUES (9999)"});
        QVERIFY(!Backup::inspect(newer, &error));
        QVERIFY(error.contains("newer version"));
    }

    void restoreBringsBackTheDataAndSavesWhatWasThere() {
        const QString backup = backupThenChange();
        QCOMPARE(accountCount(), 2);

        QVERIFY(DatabaseSession::restore(backup));
        QCOMPARE(accountCount(), 1);
        QCOMPARE(DatabaseSession::currentDatabasePath(), path);
        QVERIFY(QFile::exists(backup));                           // The backup itself stays

        const QList<Backup::AutomaticBackup> saved = Backup::automaticBackups(path);
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().kind, QString("Before restore"));

        QVERIFY(DatabaseSession::restore(saved.first().path));    // So it can be undone
        QCOMPARE(accountCount(), 2);
    }

    void theOldestAutomaticBackupCanBeRestored() {            // Saving the current data pushes it out of the kept ones
        QVERIFY(QDir().mkpath(Backup::folderFor(path)));
        const QDir backups(Backup::folderFor(path));
        for (int day = 1; day <= 5; ++day) {
            QVERIFY(Backup::write(backups.filePath(QString("budget-before-restore-2020-01-0%1-120000.db").arg(day))));
        }
        const QString oldest = backups.filePath("budget-before-restore-2020-01-01-120000.db");
        AccountDAO::createAccount(1, "Later", "Savings", 100);
        QVERIFY(DatabaseSession::restore(oldest));
        QCOMPARE(accountCount(), 1);
    }

    void aBadBackupChangesNothing() {
        QString error;
        QVERIFY(!DatabaseSession::restore(QDir(folder).filePath("missing.db"), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(accountCount(), 1);
        QVERIFY(Backup::automaticBackups(path).isEmpty());
    }

    void theOpenDatabaseCantRestoreItself() {
        QString error;
        QVERIFY(!DatabaseSession::restore(path, &error));
        QVERIFY(error.contains("open"));
        QCOMPARE(accountCount(), 1);
    }

    void aBackupThatWontOpenPutsTheDataBack() {
        // Passes the inspection, but isn't a complete database
        const QString broken = fileWith("broken.db", {"CREATE TABLE schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT)",
                                                      "INSERT INTO schema_migrations (version) VALUES (1)",
                                                      "CREATE TABLE users (id INTEGER)",
                                                      "CREATE TABLE accounts (id INTEGER)",
                                                      "CREATE TABLE transactions (id INTEGER)"});
        QVERIFY(Backup::inspect(broken).has_value());

        QString error;
        QVERIFY(!DatabaseSession::restore(broken, &error));
        QVERIFY(error.contains("put back"));
        QVERIFY(DatabaseManager::instance().database().isOpen());
        QCOMPARE(accountCount(), 1);
        QVERIFY(!QFile::exists(path + ".replaced"));
        QVERIFY(!QFile::exists(path + ".restoring"));
    }

    void listShowsThisDatabasesBackupsNewestFirst() {
        QVERIFY(QDir().mkpath(Backup::folderFor(path)));
        for (const char* name : {"budget-before-update-2026-01-01-120000.db", "budget-before-restore-2026-03-01-120000.db",
                                 "budget-before-update-2026-02-01-120000.db", "other-before-update-2026-04-01-120000.db",
                                 "budget-notes.db"}) {
            QFile file(QDir(Backup::folderFor(path)).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const QList<Backup::AutomaticBackup> list = Backup::automaticBackups(path);
        QCOMPARE(list.size(), 3);
        QCOMPARE(list[0].when, QDateTime(QDate(2026, 3, 1), QTime(12, 0)));
        QCOMPARE(list[0].kind, QString("Before restore"));
        QCOMPARE(list[1].when, QDateTime(QDate(2026, 2, 1), QTime(12, 0)));
        QCOMPARE(list[2].kind, QString("Before update"));
    }

    void restoringFromAFileAsksThenSaysSo() {
        const QString backup = backupThenChange();
        BackupSettings settings;
        QSignalSpy restored(&settings, &BackupSettings::restored);
        QString asked;
        settings.askRestorePath = [&]() { return backup; };
        settings.confirmRestore = [&](const QString& text) { asked = text; return false; };

        settings.findChild<QPushButton*>("restoreFromFileButton")->click();
        QVERIFY(asked.contains("1 user, 1 account, 1 transaction"));
        QCOMPARE(accountCount(), 2);                              // Said no
        QCOMPARE(restored.count(), 0);

        settings.confirmRestore = [](const QString&) { return true; };
        settings.findChild<QPushButton*>("restoreFromFileButton")->click();
        QCOMPARE(accountCount(), 1);
        QCOMPARE(restored.count(), 1);
    }

    void aFileThatCantBeRestoredIsntOffered() {
        BackupSettings settings;
        bool asked = false;
        const QString foreign = fileWith("foreign.db", {"CREATE TABLE notes (id INTEGER)"});
        settings.askRestorePath = [&]() { return foreign; };
        settings.confirmRestore = [&](const QString&) { asked = true; return true; };
        settings.findChild<QPushButton*>("restoreFromFileButton")->click();
        QVERIFY(!asked);
        QVERIFY(settings.findChild<QLabel*>("backupResultLabel")->text().contains("isn't an Aion"));
    }

    void rowsInTheListRestore() {
        const QString backup = backupThenChange();
        QVERIFY(DatabaseSession::restore(backup));                // Leaves a "before restore" with two accounts
        QCOMPARE(accountCount(), 1);

        BackupSettings settings;
        settings.confirmRestore = [](const QString&) { return true; };
        settings.reload();
        const QList<QPushButton*> rows = settings.findChildren<QPushButton*>("restoreBackupButton");
        QCOMPARE(rows.size(), 1);
        rows.first()->click();
        QCOMPARE(accountCount(), 2);
    }

    // --- Hostile input ---

    void aBackupWithoutMostTablesIsRefused() {
        const QString partial = dir.filePath("partial.db");
        {
            QSqlDatabase file = QSqlDatabase::addDatabase("QSQLITE", "partial");
            file.setDatabaseName(partial);
            QVERIFY(file.open());
            QSqlQuery query(file);
            for (const char* sql : {"CREATE TABLE schema_migrations (version INTEGER)", "INSERT INTO schema_migrations VALUES (1)",
                                    "CREATE TABLE users (id INTEGER)", "CREATE TABLE accounts (id INTEGER)",
                                    "CREATE TABLE transactions (id INTEGER)"}) {
                QVERIFY(query.exec(sql));
            }
            file.close();
        }
        QSqlDatabase::removeDatabase("partial");
        QString error;
        QEXPECT_FAIL("", "Not fixed yet", Continue);
        QVERIFY(!Backup::inspect(partial, &error));
    }
};

QTEST_MAIN(TestRestore)
#include "test_restore.moc"
