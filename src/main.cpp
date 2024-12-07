#include "mainwindow.h"

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
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
    MainWindow w;
    w.show();
    return a.exec();
}
