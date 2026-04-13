#pragma once

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QSqlQuery>

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "tests/test_db_utils.h" // execOrFail

/**
 * TestDbFixture
 *
 * Shared helper for DB-backed tests.
 * - Creates an isolated temporary SQLite database file
 * - Initializes schema by running the same migrations as the client
 * - Provides a place for common seed data
 *
 * Usage:
 *   TestDbFixture db;
 *   db.init();          // in initTestCase()
 *   db.seedDefaultUser(); // if needed
 */
class TestDbFixture {
public:
    void init(const QString& fileName = "test.db") {
        tempDir = std::make_unique<QTemporaryDir>();
        QVERIFY2(tempDir->isValid(), "Failed to create temporary directory for test database");

        dbPath = QDir(tempDir->path()).filePath(fileName);

        QVERIFY2(DatabaseManager::instance().openDatabase(dbPath),
                 "Failed to open test database");

        MigrationManager migrationManager(SQL_MIGRATIONS_DIR);
        QVERIFY2(migrationManager.applyMigrations(),
                 "Failed to apply migrations to test database");
    }

    void seedDefaultUser(int id = 1,
                         const QString& username = "testuser",
                         const QString& name = "Test User") {
        QSqlQuery q(DatabaseManager::instance().database());
        execOrFail(q,
                  QString("INSERT INTO users (id, username, name) VALUES (%1, '%2', '%3')")
                      .arg(id)
                      .arg(username)
                      .arg(name));
    }

    const QString& path() const { return dbPath; }

private:
    std::unique_ptr<QTemporaryDir> tempDir;
    QString dbPath;
};
