#include "mainwindow.h"

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "userselectiondialog.h"
#include <QApplication>

void setupDatabase() {
    QString dbPath = "app_data.db";
    // Open the database
    if (!DatabaseManager::instance().openDatabase(dbPath)) {
        qCritical() << "Database initialization failed!";
        return;
    }

    // Apply migrations
    MigrationManager migrationManager(SQL_MIGRATIONS_DIR);
    if (!migrationManager.applyMigrations()) {
        qCritical() << "Failed to apply migrations!";
        return;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    setupDatabase();
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
