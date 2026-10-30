#include "mainwindow.h"
#include "commandline.h"

#include "db/databasesession.h"
#include "icons.h"
#include "startwindow.h"
#include "settings.h"
#include "theme.h"
#include "user.h"
#include "systeminfo.h"
#include "version.h"
#include <QApplication>
#include <QFileInfo>
#include <QStyleHints>
#include <QTextStream>
#include <QMessageBox>
#include <cstdlib>
#include <functional>

// Opens the database for this start and says so if it isn't the one used last time
static bool openStartupDatabase() {
    QString warning;
    QString error;
    if (!DatabaseSession::openAtStartup(&warning, &error)) {
        qCritical() << "Database initialization failed:" << error;
        return false;
    }
    if (!warning.isEmpty()) {
        QMessageBox::warning(nullptr, "Aion", warning);
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
    // Only the version: setting the application/organization name would move the AppData folder
    QApplication::setApplicationVersion(AION_VERSION);

    // --version and --help, before anything opens a database
    if (argc > 1) {
        CommandLine::attachToParentConsole();
    }
    QTextStream out(stdout);
    if (const int exitCode = CommandLine::handle(QApplication::arguments(), out); exitCode >= 0) {
        return exitCode;
    }

    QApplication::setWindowIcon(Icons::appIcon());   // Every window and the taskbar
    if (!openStartupDatabase()) {
        QMessageBox::critical(nullptr, "Database Error",
                              "Aion could not initialize its database. See the application log for details.");
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
        window->setWindowTitle(QString("Aion %1 · %2").arg(SystemInfo::versionText(),
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
