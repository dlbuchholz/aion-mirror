#pragma once

#include <QtTest/QtTest>
#include <QSqlQuery>
#include <QSqlError>

/**
 * Executes an SQL statement and fails the current test immediately if it fails.
 *
 * Why this exists:
 * - QSqlQuery::exec() returns false on failure, but tests often ignore the return value,
 *   causing confusing downstream failures.
 * - This helper makes failures fail-fast and prints both the SQL and the driver error.
 *
 * Usage:
 *   QSqlQuery q(DatabaseManager::instance().database());
 *   execOrFail(q, "CREATE TABLE ...");
 */
inline void execOrFail(QSqlQuery& q, const QString& sql) {
    QVERIFY2(q.exec(sql), qPrintable(q.lastError().text() + " | SQL: " + sql));
}