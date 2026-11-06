#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>

#include "account.h"
#include "db/backup.h"
#include "tests/test_db_fixture.h"
#include "tests/test_file_dialog.h"
#include "widgets/backupsettings.h"

class TestBackup : public QObject {
    Q_OBJECT
    TestDbFixture db;
    QTemporaryDir folder;
    int giro = -1;

    // The number of transactions in another database file, by opening that file on its own
    static qlonglong transactionsIn(const QString& path) {
        qlonglong count = -1;
        {
            QSqlDatabase copy = QSqlDatabase::addDatabase("QSQLITE", "backupCheck");
            copy.setDatabaseName(path);
            if (copy.open()) {
                QSqlQuery query(copy);
                if (query.exec("SELECT COUNT(*) FROM transactions") && query.next()) {
                    count = query.value(0).toLongLong();
                }
            }
            copy.close();
        }
        QSqlDatabase::removeDatabase("backupCheck");
        return count;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 5000);
    }
    void cleanup() { db.close(); }

    void nameFollowsTheDatabaseAndTheDate() {
        QCOMPARE(Backup::suggestedName("C:/Daten/budget.db", QDate(2026, 10, 30)), QString("budget-2026-10-30.db"));
    }

    void copyHasTheDataAndLaterChangesStayOut() {
        const QString path = folder.filePath("copy.db");
        QVERIFY(Backup::write(path));
        QCOMPARE(transactionsIn(path), 1);

        AccountDAO::createAccount(1, "Tagesgeld", "Savings", 100);
        QCOMPARE(transactionsIn(path), 1);
        QVERIFY(!QFile::exists(path + ".tmp"));
    }

    void replacesAnOlderBackup() {
        const QString path = folder.filePath("again.db");
        QVERIFY(Backup::write(path));
        AccountDAO::createAccount(1, "Tagesgeld", "Savings", 100);
        QVERIFY(Backup::write(path));
        QCOMPARE(transactionsIn(path), 2);
        QVERIFY(!QFile::exists(path + ".old"));
    }

    void wontReplaceTheDatabaseItself() {
        QString error;
        QVERIFY(!Backup::write(DatabaseManager::instance().database().databaseName(), &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(AccountDAO().getAccountById(giro).getBalanceCents(), Cents(5000));
    }

    void aFailedBackupSaysSoAndLeavesNothing() {
        const QString path = folder.filePath("missing/folder/copy.db");
        QString error;
        QVERIFY(!Backup::write(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(path));
    }

    void buttonSavesWhereChosenAndAddsTheExtension() {
        BackupSettings settings;
        QString suggested;
        settings.askBackupPath = [&](const QString& path) {
            suggested = path;
            return folder.filePath("chosen");
        };
        settings.findChild<QPushButton*>("backUpNowButton")->click();

        QVERIFY(suggested.endsWith(Backup::suggestedName("case.db", QDate::currentDate())));
        QCOMPARE(transactionsIn(folder.filePath("chosen.db")), 1);
        QVERIFY(settings.findChild<QLabel*>("backupResultLabel")->text().contains("chosen.db"));
    }

    void theFileDialogsStartAtTheDatabase() {
        BackupSettings settings;
        const QString folderOfDatabase = QFileInfo(DatabaseManager::instance().database().databaseName()).absolutePath();
        const QString suggested = folder.filePath("budget-2026-11-05.db");

        // Backing up: the suggested name, as a database file
        FileDialogSeen backUp;
        answerFileDialog(&backUp, suggested);
        QCOMPARE(QFileInfo(settings.askBackupPath(suggested)), QFileInfo(suggested));
        QVERIFY(backUp.shown);
        QCOMPARE(backUp.title, QString("Back up database"));
        QCOMPARE(backUp.acceptMode, QFileDialog::AcceptSave);
        QCOMPARE(backUp.filters, QStringList({"Aion database (*.db)"}));
        QCOMPARE(QFileInfo(backUp.selected), QFileInfo(suggested));

        // Restoring: any database file, starting next to the open one
        FileDialogSeen restore;
        answerFileDialog(&restore);
        QCOMPARE(settings.askRestorePath(), QString());
        QVERIFY(restore.shown);
        QCOMPARE(restore.title, QString("Restore from file"));
        QCOMPARE(restore.fileMode, QFileDialog::ExistingFile);
        QCOMPARE(restore.folder, folderOfDatabase);
        QCOMPARE(restore.filters, QStringList({"Aion database (*.db)", "All files (*)"}));
    }

    void cancellingWritesNothing() {
        BackupSettings settings;
        settings.askBackupPath = [](const QString&) { return QString(); };
        settings.findChild<QPushButton*>("backUpNowButton")->click();
        QVERIFY(settings.findChild<QLabel*>("backupResultLabel")->text().isEmpty());
    }
};

QTEST_MAIN(TestBackup)
#include "test_backup.moc"
