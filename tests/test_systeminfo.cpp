#include <QtTest>
#include <QDir>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "systeminfo.h"
#include "version.h"

class TestSystemInfo : public QObject {
    Q_OBJECT

    QMap<QString, QVariant> originalSettings;

private slots:
    void initTestCase() {
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

    void masksTheHomeFolderInBothSeparatorStyles() {
        const QString home = QDir::homePath();
        QCOMPARE(SystemInfo::maskHomePath(home + "/data/app.db"), QString("~/data/app.db"));
        QCOMPARE(SystemInfo::maskHomePath(QDir::toNativeSeparators(home + "/data/app.db")),
                 QDir::toNativeSeparators(QString("~/data/app.db")));
        QCOMPARE(SystemInfo::maskHomePath("C:/elsewhere/app.db"), QString("C:/elsewhere/app.db"));
    }

    void versionTextShowsTheCommitInDebugBuilds() {
        const QString text = SystemInfo::versionText();
        QVERIFY(text.startsWith(AION_VERSION));
#ifdef QT_NO_DEBUG
        QCOMPARE(text, QString(AION_VERSION));
#else
        QVERIFY(text.contains("(debug"));
        if (QString(AION_GIT_COMMIT) != "unknown") QVERIFY(text.contains(AION_GIT_COMMIT));
#endif
    }

    void reportListsBuildAndDatabaseDetails() {
        // Database inside the home folder, so the report must mask its path
        QTemporaryDir dir(QDir::homePath() + "/aion-systeminfo-XXXXXX");
        QVERIFY(dir.isValid());
        QVERIFY(DatabaseSession::create(dir.filePath("report.db")));

        const QString report = SystemInfo::report();
        QVERIFY(report.startsWith("Aion\n"));
        QVERIFY(report.contains(QString("  Version: %1\n").arg(AION_VERSION)));
        QVERIFY(report.contains("  Commit: "));
        QVERIFY(report.contains("\nSystem\n"));
        QVERIFY(report.contains("\nData\n"));
        QVERIFY(report.contains("  Schema version: 1\n"));
        QVERIFY(report.contains("report.db"));
        QVERIFY(!report.contains(QDir::toNativeSeparators(QDir::homePath()), Qt::CaseInsensitive));

        // The on-screen entries keep the full path
        bool foundFullPath = false;
        for (const SystemInfo::Entry& entry : SystemInfo::entries()) {
            if (entry.label == "Database") {
                foundFullPath = entry.value == QDir::toNativeSeparators(QDir::cleanPath(dir.filePath("report.db")));
            }
        }
        QVERIFY(foundFullPath);
        DatabaseManager::instance().closeDatabase();
    }
};

QTEST_MAIN(TestSystemInfo)
#include "test_systeminfo.moc"
