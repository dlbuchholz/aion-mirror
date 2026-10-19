#include "balance.h"
#include "category.h"
#include "db/databasemanager.h"
#include "transaction.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Balance {

Cents onDate(int accountId, const QDate& date) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COALESCE(SUM(CASE WHEN type = 'income' THEN amount ELSE -amount END), 0) "
                  "FROM transactions WHERE account_id = :account_id AND date <= :date");
    query.bindValue(":account_id", accountId);
    query.bindValue(":date", date.toString("yyyy-MM-dd"));
    if (!query.exec() || !query.next()) {
        qWarning() << "Balance::onDate:" << query.lastError().text();
        return 0;
    }
    return query.value(0).toLongLong();
}

int addCorrection(int accountId, const QDate& date, Cents differenceCents, const QString& note) {
    if (differenceCents == 0) {
        return 0;
    }
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id FROM categories WHERE name = :name");
    query.bindValue(":name", BalanceCorrectionCategory);
    const int categoryId = query.exec() && query.next() ? query.value(0).toInt() : 0;   // No category if it was deleted

    const Transaction correction(-1, accountId, BalanceCorrectionCategory, qAbs(differenceCents), date, note, categoryId,
                                 differenceCents > 0 ? "income" : "expense");
    return TransactionDAO().addTransaction(correction);
}

} // namespace Balance
