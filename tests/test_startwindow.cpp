#include <QtTest>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "startwindow.h"
#include "user.h"
#include "version.h"

class TestStartWindow : public QObject {
    Q_OBJECT

    QTemporaryDir dir;
    QMap<QString, QVariant> originalSettings;
    int databaseCounter = 0;

    static void fillForm(StartWindow& window, const QString& name, const QString& username) {
        window.findChild<QLineEdit*>("nameEdit")->setText(name);
        window.findChild<QLineEdit*>("usernameEdit")->setText(username);
        window.findChild<QPushButton*>("addUserButton")->click();
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
        // Every test gets its own fresh database
        QVERIFY(DatabaseSession::create(dir.filePath(QString("case%1.db").arg(++databaseCounter))));
        Settings::instance().setDefaultUserId(-1);
    }

    void emptyDatabaseOpensAddForm() {
        StartWindow window;
        QVERIFY(!window.findChild<QLineEdit*>("nameEdit")->parentWidget()->isHidden());
    }

    void showsTheBuildVersion() {
        StartWindow window;
        QCOMPARE(window.findChild<QLabel*>("versionLabel")->text(), QString("Version %1").arg(AION_VERSION));
        QVERIFY(QString(AION_VERSION).count('.') == 2);    // Major.minor.patch from CMake
    }

    void addingAUserOpensIt() {
        StartWindow window;
        fillForm(window, "Alex Example", "alex");

        QCOMPARE(window.result(), int(QDialog::Accepted));
        const int id = UserDAO().findUserIdByUsername("alex");
        QVERIFY(id > 0);
        QCOMPARE(window.selectedUserId(), id);
        QCOMPARE(Settings::instance().getDefaultUserId(), -1);   // Not remembered unless asked
    }

    void rememberStoresTheUser() {
        StartWindow window;
        window.findChild<QCheckBox*>()->setChecked(true);
        fillForm(window, "Sam Example", "sam");
        QCOMPARE(Settings::instance().getDefaultUserId(), window.selectedUserId());
    }

    void invalidOrDuplicateUsersShowAnError() {
        QVERIFY(UserDAO().createUser(User(0, "alex", "Alex Example")));
        StartWindow window;
        // Existing users: the form starts hidden
        QVERIFY(window.findChild<QLineEdit*>("nameEdit")->parentWidget()->isHidden());

        fillForm(window, "", "someone");
        QCOMPARE(window.result(), 0);

        fillForm(window, "Other Name", "alex");
        QCOMPARE(window.result(), 0);
        QCOMPARE(UserDAO().getAllUsers().size(), 1);
    }
};

QTEST_MAIN(TestStartWindow)
#include "test_startwindow.moc"
