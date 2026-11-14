#include <QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "appname.h"
#include "namemigration.h"

using NameMigration::Outcome;

// Each test gets its own folders and its own settings (in the test program's isolated settings
// folder), named after the old and the new application
class TestNameMigration : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> root;
    NameMigration::Places places;
    int run = 0;

    QSettings oldSettings() const { return {QSettings::defaultFormat(), QSettings::UserScope, places.organization, places.oldApplication}; }
    QSettings newSettings() const { return {QSettings::defaultFormat(), QSettings::UserScope, places.organization, places.newApplication}; }

    static void write(const QString& path, const QByteArray& contents) {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
    static QByteArray read(const QString& path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // What someone who used the old name has: the default database with backups, and settings
    // that point into the folder and outside it
    void useOldName() {
        write(places.oldDataFolder + "/app_data.db", "database");
        write(places.oldDataFolder + "/backups/app_data-2026-11-01-120000.db", "backup");
        write(places.oldDataFolder + "/.hidden", "hidden");
        QSettings old = oldSettings();
        old.setValue("app/language", "de");
        old.setValue("app/lastDatabase", places.oldDataFolder + "/app_data.db");
        old.setValue("app/recentDatabases", QStringList{places.oldDataFolder + "/app_data.db", "D:/Finanzen/family.db"});
        old.setValue("app/defaultUsers", QVariantMap{{places.oldDataFolder + "/app_data.db", 2}, {"D:/Finanzen/family.db", 5}});
        old.setValue("backup/failures", QVariantMap{{places.oldDataFolder + "/app_data.db",
                                                     QVariantMap{{"when", QDateTime(QDate(2026, 11, 1), QTime(12, 0))}, {"reason", "Disk full"}}}});
        old.sync();
    }

private slots:
    void init() {
        root = std::make_unique<QTemporaryDir>();
        QVERIFY(root->isValid());
        ++run;
        places = {root->path() + "/Old", root->path() + "/New", "MigrationTest",
                  QString("Old%1").arg(run), QString("New%1").arg(run)};
    }

    void cleanup() {
        oldSettings().clear();
        newSettings().clear();
    }

    void standardPlacesAreTheAppDataFoldersOfBothNames() {
        QStandardPaths::setTestModeEnabled(true);
        const QString name = QCoreApplication::applicationName();
        const NameMigration::Places standard = NameMigration::standardPlaces();
        QCOMPARE(QCoreApplication::applicationName(), name);              // Put back as it was
        QCOMPARE(standard.newDataFolder, QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
        QVERIFY2(standard.oldDataFolder.endsWith("/Aion"), qPrintable(standard.oldDataFolder));
        QCOMPARE(QFileInfo(standard.oldDataFolder).path(), QFileInfo(standard.newDataFolder).path());   // Side by side
        QCOMPARE(standard.organization, QString(APP_ORGANIZATION));
        QCOMPARE(standard.oldApplication, QString("Aion"));
        QCOMPARE(standard.newApplication, QString(APP_NAME));
    }

    void movesPathsInTheOldFolder() {
        const QString from = "C:/Users/alex/AppData/Roaming/Aion";
        const QString to = "C:/Users/alex/AppData/Roaming/Liora";
        using NameMigration::movedPaths;
        QCOMPARE(movedPaths(from + "/app_data.db", from, to), QVariant(to + "/app_data.db"));
        QCOMPARE(movedPaths(from + "/backups/a.db", from, to), QVariant(to + "/backups/a.db"));
        QCOMPARE(movedPaths(from, from, to), QVariant(to));
        QCOMPARE(movedPaths("D:/Finanzen/family.db", from, to), QVariant("D:/Finanzen/family.db"));
        QCOMPARE(movedPaths(from + "2/app_data.db", from, to), QVariant(from + "2/app_data.db"));      // Another folder
        QCOMPARE(movedPaths("Aion", from, to), QVariant("Aion"));
        QCOMPARE(movedPaths(42, from, to), QVariant(42));
        QCOMPARE(movedPaths(true, from, to), QVariant(true));

        QCOMPARE(movedPaths(QStringList{from + "/a.db", "D:/b.db"}, from, to), QVariant(QStringList{to + "/a.db", "D:/b.db"}));
        QCOMPARE(movedPaths(QVariantList{from + "/a.db", 3}, from, to), QVariant(QVariantList{to + "/a.db", 3}));
        const QVariant nested = movedPaths(QVariantMap{{from + "/a.db", QVariantMap{{"file", from + "/b.db"}, {"count", 1}}},
                                                       {"D:/c.db", 7}}, from, to);
        QCOMPARE(nested, QVariant(QVariantMap{{to + "/a.db", QVariantMap{{"file", to + "/b.db"}, {"count", 1}}},
                                              {"D:/c.db", 7}}));
    }

    void pathCaseFollowsTheSystem() {
        const QVariant moved = NameMigration::movedPaths("c:/users/alex/appdata/roaming/aion/app_data.db",
                                                         "C:/Users/alex/AppData/Roaming/Aion", "C:/Users/alex/AppData/Roaming/Liora");
#ifdef Q_OS_WIN
        QCOMPARE(moved, QVariant("C:/Users/alex/AppData/Roaming/Liora/app_data.db"));   // Windows paths ignore case
#else
        QCOMPARE(moved, QVariant("c:/users/alex/appdata/roaming/aion/app_data.db"));
#endif
    }

    void nothingUnderTheOldNameChangesNothing() {
        QString error;
        QCOMPARE(NameMigration::carryOver(places, &error), Outcome::NotNeeded);
        QVERIFY(error.isEmpty());
        QVERIFY(newSettings().allKeys().isEmpty());
        QVERIFY(!QFileInfo::exists(places.newDataFolder));
    }

    void anEmptyOldFolderIsNothingToCarry() {
        QVERIFY(QDir().mkpath(places.oldDataFolder));
        QCOMPARE(NameMigration::carryOver(places), Outcome::NotNeeded);
        QVERIFY(!QFileInfo::exists(places.newDataFolder));
    }

    void carriesOverTheFolderAndSettings() {
        useOldName();
        QString error;
        QCOMPARE(NameMigration::carryOver(places, &error), Outcome::CarriedOver);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // The whole folder, hidden files and backups too
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("database"));
        QCOMPARE(read(places.newDataFolder + "/backups/app_data-2026-11-01-120000.db"), QByteArray("backup"));
        QCOMPARE(read(places.newDataFolder + "/.hidden"), QByteArray("hidden"));
        QVERIFY(!QFileInfo::exists(places.newDataFolder + ".partial"));

        // Every setting, with the paths into the old folder now into the new one
        const QSettings now = newSettings();
        QCOMPARE(now.value("app/language").toString(), QString("de"));
        QCOMPARE(now.value("app/lastDatabase").toString(), places.newDataFolder + "/app_data.db");
        QCOMPARE(now.value("app/recentDatabases").toStringList(), QStringList({places.newDataFolder + "/app_data.db", "D:/Finanzen/family.db"}));
        QCOMPARE(now.value("app/defaultUsers").toMap(), QVariantMap({{places.newDataFolder + "/app_data.db", 2}, {"D:/Finanzen/family.db", 5}}));
        const QVariantMap failure = now.value("backup/failures").toMap().value(places.newDataFolder + "/app_data.db").toMap();
        QCOMPARE(failure.value("reason").toString(), QString("Disk full"));
        QCOMPARE(failure.value("when").toDateTime(), QDateTime(QDate(2026, 11, 1), QTime(12, 0)));
        QCOMPARE(now.value("app/migratedFrom").toString(), places.oldApplication);

        // The old name's data stays, as a backup
        QCOMPARE(read(places.oldDataFolder + "/app_data.db"), QByteArray("database"));
        QCOMPARE(oldSettings().value("app/lastDatabase").toString(), places.oldDataFolder + "/app_data.db");
    }

    void carriesOverOnce() {
        useOldName();
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        write(places.newDataFolder + "/app_data.db", "changed under the new name");
        QSettings old = oldSettings();
        old.setValue("app/language", "en");
        old.sync();

        QCOMPARE(NameMigration::carryOver(places), Outcome::NotNeeded);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("changed under the new name"));
        QCOMPARE(newSettings().value("app/language").toString(), QString("de"));
    }

    void leavesANewNameInUseAlone() {
        useOldName();
        QSettings used = newSettings();
        used.setValue("app/language", "en");
        used.sync();

        QCOMPARE(NameMigration::carryOver(places), Outcome::NotNeeded);
        QVERIFY(!QFileInfo::exists(places.newDataFolder));
        QCOMPARE(newSettings().allKeys(), QStringList{"app/language"});
    }

    void settingsWithoutAFolder() {
        QSettings old = oldSettings();
        old.setValue("app/lastDatabase", "D:/Finanzen/family.db");
        old.sync();

        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(newSettings().value("app/lastDatabase").toString(), QString("D:/Finanzen/family.db"));
        QVERIFY(!QFileInfo::exists(places.newDataFolder));
    }

    void aFolderWithoutSettings() {
        write(places.oldDataFolder + "/app_data.db", "database");
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("database"));
        QCOMPARE(newSettings().value("app/migratedFrom").toString(), places.oldApplication);
    }

    void anEmptyNewFolderIsFilled() {
        useOldName();
        QVERIFY(QDir().mkpath(places.newDataFolder));
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("database"));
    }

    // A start that stopped after the folder was copied, before the settings: the folder is kept
    void finishesAnEarlierStart() {
        useOldName();
        write(places.newDataFolder + "/app_data.db", "copied before");
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("copied before"));
        QVERIFY(!QFileInfo::exists(places.newDataFolder + "/backups"));
        QCOMPARE(newSettings().value("app/lastDatabase").toString(), places.newDataFolder + "/app_data.db");
    }

    // A start that stopped while copying: its half copy is replaced
    void replacesAHalfCopy() {
        useOldName();
        write(places.newDataFolder + ".partial/app_data.db", "half");
        write(places.newDataFolder + ".partial/stray.db", "stray");
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("database"));
        QVERIFY(!QFileInfo::exists(places.newDataFolder + "/stray.db"));
        QVERIFY(!QFileInfo::exists(places.newDataFolder + ".partial"));
    }

    // When the folder can't be copied, nothing changes, so the next start tries again
    void aFailedCopyChangesNothing() {
        useOldName();
        write(root->path() + "/blocked", "a file, not a folder");
        places.newDataFolder = root->path() + "/blocked/New";

        QString error;
        QCOMPARE(NameMigration::carryOver(places, &error), Outcome::Failed);
        QVERIFY2(error.startsWith("Couldn't create"), qPrintable(error));
        QVERIFY(newSettings().allKeys().isEmpty());
        QCOMPARE(read(places.oldDataFolder + "/app_data.db"), QByteArray("database"));
        QCOMPARE(oldSettings().value("app/language").toString(), QString("de"));

        places.newDataFolder = root->path() + "/New";                       // Fixed: the next start carries it over
        QCOMPARE(NameMigration::carryOver(places), Outcome::CarriedOver);
        QCOMPARE(read(places.newDataFolder + "/app_data.db"), QByteArray("database"));
    }
};

QTEST_MAIN(TestNameMigration)
#include "test_namemigration.moc"
