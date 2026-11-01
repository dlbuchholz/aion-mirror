#pragma once

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QSqlQuery>

#include "db/databasemanager.h"
#include "db/migrationmanager.h"
#include "user.h"
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

        MigrationManager mm(BuiltInMigrations);
        QVERIFY2(mm.applyMigrations(), "migrations failed");

        DatabaseManager::instance().closeDatabase();
    }

    void yankDb(const QString& fileName = "case.db") {
        QVERIFY2(!templatePath.isEmpty(), "Template DB not initialized");

        casePath = QDir(rootDir->path()).filePath(fileName);
        QFile::remove(casePath);
        QVERIFY2(QFile::copy(templatePath, casePath), "Failed to copy template DB");

        QVERIFY2(DatabaseManager::instance().openDatabase(casePath), "open case db failed");

    }

    void close() {
        DatabaseManager::instance().closeDatabase();
    }

    void seedDefaultUser(int id = 1,
                         const QString& username = "testuser",
                         const QString& name = "Test User") {
        // Like a real new user, with the default account types and categories
        QVERIFY2(UserDAO().createUser(User(id, username, name)), "seeding the user failed");
    }

    // Makes the database refuse this kind of write from now on (INSERT, UPDATE or DELETE on the
    // table, if the condition holds), as it could with a full disk. Reading still works.
    static bool refuse(const QString& operation, const QString& table, const QString& condition = "1") {
        QSqlQuery query(DatabaseManager::instance().database());
        return query.exec(QString("CREATE TRIGGER refuse_%1_%2 BEFORE %1 ON %2 WHEN %3 BEGIN SELECT RAISE(ABORT, 'refused'); END")
                              .arg(operation, table, condition));
    }

    // Makes committing fail after this kind of write: it leaves a row that breaks a deferred foreign
    // key, which SQLite only checks at the commit
    static bool failCommitAfter(const QString& operation, const QString& table) {
        QSqlQuery query(DatabaseManager::instance().database());
        return query.exec("CREATE TABLE IF NOT EXISTS commit_guard (user_id INTEGER REFERENCES users(id) DEFERRABLE INITIALLY DEFERRED)")
               && query.exec(QString("CREATE TRIGGER fail_commit_%1_%2 AFTER %1 ON %2 BEGIN INSERT INTO commit_guard VALUES (-1); END")
                                 .arg(operation, table));
    }

    // Removes a table, so that everything reading it fails
    static bool dropTable(const QString& table) {
        QSqlQuery query(DatabaseManager::instance().database());
        return query.exec("PRAGMA foreign_keys = OFF") && query.exec("DROP TABLE " + table);
    }

    const QString& path() const { return templatePath; }

private:
    std::unique_ptr<QTemporaryDir> rootDir;
    QString templatePath;
    QString casePath;
};
