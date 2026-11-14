#include "mainwindow.h"
#include "appname.h"
#include "commandline.h"

#include "db/databasesession.h"
#include "icons.h"
#include "language.h"
#include "namemigration.h"
#include "startwindow.h"
#include "settings.h"
#include "theme.h"
#include "user.h"
#include "systeminfo.h"
#include "version.h"
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QStyleHints>
#include <QTextStream>
#include <QMessageBox>
#include <cstdlib>
#include <functional>

// Data and settings from the program's earlier name, the first time it starts under this one.
// Returns false when copying them failed: then nothing may save settings, or the next start
// would take the new name as used already and not try again.
static bool carryOverFromPreviousName() {
    QString error;
    if (NameMigration::carryOver(NameMigration::standardPlaces(), &error) != NameMigration::Outcome::Failed) {
        return true;
    }
    qCritical() << "Couldn't carry over the data of" << NameMigration::PreviousName << ":" << error;
    // The language from the old settings: Settings would save its defaults in the new ones
    const QSettings previous(QSettings::defaultFormat(), QSettings::UserScope, APP_ORGANIZATION, NameMigration::PreviousName);
    Language::install(Language::resolve(previous.value("app/language", "system").toString()));
    QMessageBox::critical(nullptr, APP_NAME,
                          QCoreApplication::translate("NameMigration", "%1 was called %2 before, and couldn't take over "
                                                                       "your data and settings from it. %3\n\nNothing was "
                                                                       "changed or lost. Start %1 again to try once more.")
                              .arg(APP_NAME, NameMigration::PreviousName, error));
    return false;
}

// Opens the database for this start and says so if it isn't the one used last time
static bool openStartupDatabase() {
    QString warning;
    QString error;
    if (!DatabaseSession::openAtStartup(&warning, &error)) {
        qCritical() << "Database initialization failed:" << error;
        QMessageBox::critical(nullptr, APP_NAME, error);
        return false;
    }
    if (!warning.isEmpty()) {
        QMessageBox::warning(nullptr, APP_NAME, warning);
    }
    return true;
}

// The user to open without asking, if one was remembered and exists in this database
static int rememberedUserId() {
    const int userId = Settings::instance().getDefaultUserId(DatabaseSession::currentDatabasePath());
    if (userId <= 0) {
        return -1;
    }
    UserDAO userDao;
    return userDao.getUserById(userId).getUserId();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // The app data folder is named after the application (no organization, so it's just the name)
    QApplication::setApplicationName(APP_NAME);
    QApplication::setApplicationVersion(APP_VERSION);

    // --version and --help, before anything opens a database
    if (argc > 1) {
        CommandLine::attachToParentConsole();
    }
    QTextStream out(stdout);
    if (const int exitCode = CommandLine::handle(QApplication::arguments(), out); exitCode >= 0) {
        return exitCode;
    }

    // Before anything reads the settings or opens the default database
    if (!carryOverFromPreviousName()) {
        return EXIT_FAILURE;
    }

    // The language of the texts, before the first window or message (the command line stays English)
    Language::install(Language::resolve(Settings::instance().getLanguage()));

    QApplication::setWindowIcon(Icons::appIcon());   // Every window and the taskbar
    if (!openStartupDatabase()) {
        return EXIT_FAILURE;
    }
    // Apply the configured theme; "System" also follows later OS light/dark switches
    Theme::applyTheme(Theme::fromName(Settings::instance().getTheme()));
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &a, []() {
        if (Theme::fromName(Settings::instance().getTheme()) == Theme::Type::System) {
            Theme::applyTheme(Theme::Type::System);
        }
    });

    // Opens the main window for a user. "Switch user" closes it and returns to the start window.
    std::function<void(int)> openMainWindow = [&openMainWindow](int userId) {
        auto* window = new MainWindow(userId);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setWindowTitle(QString("%1 %2 · %3").arg(APP_NAME, SystemInfo::versionText(),
                                                         QFileInfo(DatabaseSession::currentDatabasePath()).fileName()));
        QObject::connect(window, &MainWindow::switchUserRequested, window, [window, &openMainWindow]() {
            window->hide();
            StartWindow startWindow;
            const bool chosen = startWindow.exec() == QDialog::Accepted;
            if (chosen) {
                openMainWindow(startWindow.selectedUserId());
            }
            window->close();
            if (!chosen) {
                QApplication::quit();
            }
        });
        window->show();
    };

    int userId = rememberedUserId();
    if (userId <= 0) {
        StartWindow startWindow;
        if (startWindow.exec() != QDialog::Accepted) {
            return 0; // Exit the application if no user was selected
        }
        userId = startWindow.selectedUserId();
    }
    openMainWindow(userId);
    return a.exec();
}
