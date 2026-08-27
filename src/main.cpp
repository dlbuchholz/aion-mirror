#include "mainwindow.h"

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "userselectiondialog.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <cstdlib>

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
    UserSelectionDialog userSelectionDialog;

    if (userSelectionDialog.exec() != QDialog::Accepted) {
        qDebug() << "No user selected. Exiting.";
        return 0; // Exit the application if no user was selected
    }

    int userId = userSelectionDialog.getSelectedUserId();
    qDebug() << "User ID selected:" << userId;

    MainWindow w(userId);
    w.show();
    return a.exec();
}
