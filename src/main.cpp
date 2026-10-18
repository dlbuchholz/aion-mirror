#include "mainwindow.h"

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "userselectiondialog.h"
#include "settings.h"
#include "theme.h"
#include <QApplication>
#include <QStyleHints>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <cstdlib>
#include <functional>

static bool setupDatabase() {
    QString basePath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QDir().mkpath(basePath);

    QString dbPath = basePath + "/app_data.db";
    // Open the database
    if (!DatabaseManager::instance().openDatabase(dbPath)) {
        qCritical() << "Database initialization failed!";
        return false;
    }

    // Apply migrations
    MigrationManager migrationManager(SQL_MIGRATIONS_DIR);
    if (!migrationManager.applyMigrations()) {
        qCritical() << "Failed to apply migrations!";
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if (!setupDatabase()) {
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

    // Asks for a user and opens their main window. Returns false if the selection was cancelled.
    std::function<bool()> openMainWindow = [&openMainWindow]() {
        UserSelectionDialog userSelectionDialog;
        if (userSelectionDialog.exec() != QDialog::Accepted) {
            qDebug() << "No user selected.";
            return false;
        }

        int userId = userSelectionDialog.getSelectedUserId();
        qDebug() << "User ID selected:" << userId;

        auto* window = new MainWindow(userId);
        window->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(window, &MainWindow::switchUserRequested, window, [window, &openMainWindow]() {
            // Keep the current window open if switching is cancelled
            if (openMainWindow()) {
                window->close();
            }
        });
        window->show();
        return true;
    };

    if (!openMainWindow()) {
        return 0; // Exit the application if no user was selected
    }
    return a.exec();
}
