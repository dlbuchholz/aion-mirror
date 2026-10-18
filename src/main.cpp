#include "mainwindow.h"

#include "db/databasesession.h"
#include "startwindow.h"
#include "settings.h"
#include "theme.h"
#include "user.h"
#include "systeminfo.h"
#include "version.h"
#include <QApplication>
#include <QFileInfo>
#include <QStyleHints>
#include <QMessageBox>
#include <cstdlib>
#include <functional>

// Opens the database used last time, or the default one if that fails
static bool openStartupDatabase() {
    const QString last = Settings::instance().getLastDatabase();
    if (!last.isEmpty()) {
        QString error;
        if (DatabaseSession::open(last, &error)) {
            return true;
        }
        qWarning() << "Could not open last database" << last << ":" << error;
    }

    QString error;
    if (!DatabaseSession::open(DatabaseSession::defaultDatabasePath(), &error)) {
        qCritical() << "Database initialization failed:" << error;
        return false;
    }
    return true;
}

// The user to open without asking, if one was remembered and exists in this database
static int rememberedUserId() {
    const int userId = Settings::instance().getDefaultUserId();
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
