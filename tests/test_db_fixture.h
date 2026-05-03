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
 *   db.yankDb()           // yeet all DB data for new test
 *   db.seedDefaultUser(); // if needed
 */
class TestDbFixture {
public:
    void init() {
        rootDir = std::make_unique<QTemporaryDir>();
        QVERIFY2(rootDir->isValid(), "Failed to create temp dir");

        templatePath = QDir(rootDir->path()).filePath("template.db");

        QVERIFY2(DatabaseManager::instance().openDatabase(templatePath), "open template db failed");

        QSqlQuery pragma(DatabaseManager::instance().database());
        execOrFail(pragma, "PRAGMA foreign_keys = ON");

        MigrationManager mm(SQL_MIGRATIONS_DIR);
        QVERIFY2(mm.applyMigrations(), "migrations failed");

        DatabaseManager::instance().closeDatabase();
    }

    void yankDb(const QString& fileName = "case.db") {
        QVERIFY2(!templatePath.isEmpty(), "Template DB not initialized");

        casePath = QDir(rootDir->path()).filePath(fileName);
        QFile::remove(casePath);
        QVERIFY2(QFile::copy(templatePath, casePath), "Failed to copy template DB");

        QVERIFY2(DatabaseManager::instance().openDatabase(casePath), "open case db failed");

        QSqlQuery pragma(DatabaseManager::instance().database());
        execOrFail(pragma, "PRAGMA foreign_keys = ON");
    }

    void close() {
        DatabaseManager::instance().closeDatabase();
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

    const QString& path() const { return templatePath; }

private:
    std::unique_ptr<QTemporaryDir> rootDir;
    QString templatePath;
    QString casePath;
};
