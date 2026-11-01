#include <QtTest>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractButton>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "startwindow.h"
#include "systeminfo.h"
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

    static int remembered() {
        return Settings::instance().getDefaultUserId(DatabaseSession::currentDatabasePath());
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
        Settings::instance().getQSettings().remove("app/defaultUsers");
    }

    void emptyDatabaseOpensAddForm() {
        StartWindow window;
        QVERIFY(!window.findChild<QLineEdit*>("nameEdit")->parentWidget()->isHidden());
    }

    void showsTheBuildVersion() {
        StartWindow window;
        QCOMPARE(window.findChild<QLabel*>("versionLabel")->text(), QString("Version %1").arg(SystemInfo::versionText()));
        QVERIFY(QString(AION_VERSION).count('.') == 2);    // Major.minor.patch from CMake
    }

    void addingAUserOpensIt() {
        StartWindow window;
        fillForm(window, "Alex Example", "alex");

        QCOMPARE(window.result(), int(QDialog::Accepted));
        const int id = UserDAO().findUserIdByUsername("alex");
        QVERIFY(id > 0);
        QCOMPARE(window.selectedUserId(), id);
        QCOMPARE(remembered(), -1);   // Not remembered unless asked
    }

    void rememberStoresTheUser() {
        StartWindow window;
        window.findChild<QCheckBox*>()->setChecked(true);
        fillForm(window, "Sam Example", "sam");
        QCOMPARE(remembered(), window.selectedUserId());
    }

    // --- Switching databases ---

    static QMenu* databaseMenu(StartWindow& window) {
        QMenu* menu = window.findChild<QToolButton*>("databaseButton")->menu();
        emit menu->aboutToShow();                    // Built when it opens
        return menu;
    }

    static QAction* menuEntry(QMenu* menu, const QString& text) {
        for (QAction* action : menu->actions()) {
            if (action->text() == text) return action;
        }
        return nullptr;
    }

    static bool showsUser(StartWindow& window, const QString& name) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        for (QAbstractButton* card : window.findChildren<QAbstractButton*>()) {
            if (card->accessibleName() == name && !card->isHidden()) return true;
        }
        return false;
    }

    void menuListsRecentDatabasesAndForgetsMissingOnes() {
        const QString other = dir.filePath("other.db");
        QVERIFY(DatabaseSession::create(other));
        const QString current = dir.filePath(QString("case%1.db").arg(++databaseCounter));
        QVERIFY(DatabaseSession::create(current));
        const QString gone = dir.filePath("gone.db");
        Settings::instance().addRecentDatabase(gone);                     // File doesn't exist

        StartWindow window;
        QCOMPARE(window.findChild<QToolButton*>("databaseButton")->text(), QFileInfo(current).fileName());
        QMenu* menu = databaseMenu(window);
        QVERIFY(menuEntry(menu, "other.db"));
        QVERIFY(menuEntry(menu, QFileInfo(current).fileName())->isChecked());
        QVERIFY(!menuEntry(menu, "other.db")->isChecked());
        QVERIFY(!menuEntry(menu, "gone.db"));
        QVERIFY(!Settings::instance().getRecentDatabases().contains(gone));
        QVERIFY(menuEntry(menu, "Open database file…"));
        QVERIFY(menuEntry(menu, "New database…"));
    }

    void switchingShowsTheOtherDatabasesUsers() {
        const QString other = dir.filePath("people.db");
        QVERIFY(DatabaseSession::create(other));
        QVERIFY(UserDAO().createUser(User(0, "anna", "Anna Other")));
        QVERIFY(DatabaseSession::create(dir.filePath(QString("case%1.db").arg(++databaseCounter))));
        QVERIFY(UserDAO().createUser(User(0, "bob", "Bob Here")));

        StartWindow window;
        QVERIFY(showsUser(window, "Bob Here"));
        menuEntry(databaseMenu(window), "people.db")->trigger();
        QCOMPARE(DatabaseSession::currentDatabasePath(), other);
        QCOMPARE(window.findChild<QToolButton*>("databaseButton")->text(), QString("people.db"));
        QVERIFY(showsUser(window, "Anna Other"));
        QVERIFY(!showsUser(window, "Bob Here"));
    }

    void theRememberedUserBelongsToItsDatabase() {
        // User 1 in another database is someone else, so it must not open automatically there
        const QString other = dir.filePath("household.db");
        QVERIFY(DatabaseSession::create(other));
        QVERIFY(UserDAO().createUser(User(0, "kim", "Kim Other")));
        QVERIFY(DatabaseSession::create(dir.filePath(QString("case%1.db").arg(++databaseCounter))));
        {
            StartWindow window;
            window.findChild<QCheckBox*>()->setChecked(true);
            fillForm(window, "Sam Example", "sam");
            QCOMPARE(remembered(), window.selectedUserId());
        }

        StartWindow window;
        auto* rememberCheck = window.findChild<QCheckBox*>();
        QVERIFY(rememberCheck->isChecked());
        menuEntry(databaseMenu(window), "household.db")->trigger();
        QCOMPARE(remembered(), -1);
        QVERIFY(!rememberCheck->isChecked());
    }

    void aBrokenDatabaseFileIsExplained() {
        const QString current = DatabaseSession::currentDatabasePath();
        const QString broken = dir.filePath("broken.db");
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is not a database");
        file.close();
        Settings::instance().addRecentDatabase(broken);

        StartWindow window;
        menuEntry(databaseMenu(window), "broken.db")->trigger();
        auto* message = window.findChild<QLabel*>("startMessageLabel");
        QVERIFY(!message->isHidden());
        QVERIFY(message->text().startsWith("Couldn't open broken.db."));
        QCOMPARE(DatabaseSession::currentDatabasePath(), current);         // Still the old one
    }

    void openingAFileSwitchesToIt() {
        const QString current = DatabaseSession::currentDatabasePath();
        const QString other = dir.filePath("chosen.db");
        QVERIFY(DatabaseSession::create(other));
        QVERIFY(UserDAO().createUser(User(0, "anna", "Anna Other")));
        QVERIFY(DatabaseSession::open(current));

        StartWindow window;
        QString answer;                                  // Cancelled first
        window.askDatabaseToOpen = [&answer]() { return answer; };
        menuEntry(databaseMenu(window), "Open database file…")->trigger();
        QCOMPARE(DatabaseSession::currentDatabasePath(), current);

        answer = other;
        menuEntry(databaseMenu(window), "Open database file…")->trigger();
        QCOMPARE(DatabaseSession::currentDatabasePath(), other);
        QVERIFY(showsUser(window, "Anna Other"));
    }

    void aNewDatabaseGetsItsEndingAndOpens() {
        const QString current = DatabaseSession::currentDatabasePath();
        QVERIFY(UserDAO().createUser(User(0, "bob", "Bob Here")));
        StartWindow window;
        QString answer;                                  // Cancelled first
        window.askDatabaseToCreate = [&answer]() { return answer; };
        menuEntry(databaseMenu(window), "New database…")->trigger();
        QCOMPARE(DatabaseSession::currentDatabasePath(), current);
        QVERIFY(showsUser(window, "Bob Here"));

        answer = dir.filePath("family");
        menuEntry(databaseMenu(window), "New database…")->trigger();
        QCOMPARE(DatabaseSession::currentDatabasePath(), dir.filePath("family.db"));
        QVERIFY(QFile::exists(dir.filePath("family.db")));
        QCOMPARE(window.findChild<QToolButton*>("databaseButton")->text(), QString("family.db"));
        QVERIFY(!showsUser(window, "Bob Here"));
        QVERIFY(!window.findChild<QLineEdit*>("nameEdit")->parentWidget()->isHidden());   // No users yet
    }

    void aNewDatabaseOverAnExistingFileIsRefused() {
        // The file dialog doesn't warn about overwriting, so this must never touch the file
        const QString existing = dir.filePath("existing.db");
        QVERIFY(DatabaseSession::create(existing));
        QVERIFY(UserDAO().createUser(User(0, "anna", "Anna Other")));
        const QString current = dir.filePath(QString("case%1.db").arg(++databaseCounter));
        QVERIFY(DatabaseSession::create(current));

        StartWindow window;
        window.askDatabaseToCreate = [&existing]() { return existing; };
        menuEntry(databaseMenu(window), "New database…")->trigger();
        auto* message = window.findChild<QLabel*>("startMessageLabel");
        QVERIFY(!message->isHidden());
        QVERIFY(message->text().startsWith("Couldn't create existing.db."));
        QCOMPARE(DatabaseSession::currentDatabasePath(), current);         // Still the old one

        QVERIFY(DatabaseSession::open(existing));
        QVERIFY(UserDAO().findUserIdByUsername("anna") > 0);
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
