#include "db/databasemanager.h"
#include "transaction.h"

// Transaction Implementation
Transaction::Transaction(int id, double amount, const QDate& date, const QString& description, const QString& category, const QString& type)
    : transactionId(id), amount(amount), date(date), description(description), category(category), type(type) {}

int Transaction::getTransactionId() const {
    return transactionId;
}

double Transaction::getAmount() const {
    return amount;
}

QDate Transaction::getDate() const {
    return date;
}

QString Transaction::getDescription() const {
    return description;
}

QString Transaction::getCategory() const {
    return category;
}

QString Transaction::getType() const {
    return type;
}

// TransactionDAO Implementation
TransactionDAO::TransactionDAO() {}

bool TransactionDAO::addTransaction(const Transaction& transaction) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "INSERT INTO transactions (id, amount, date, description, category, type) "
        "VALUES (:id, :amount, :date, :description, :category, :type)"
        );
    query.bindValue(":id", transaction.getTransactionId());
    query.bindValue(":amount", transaction.getAmount());
    query.bindValue(":date", transaction.getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":category", transaction.getCategory());
    query.bindValue(":type", transaction.getType());

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

QList<Transaction> TransactionDAO::getTransactionsForAccount(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, amount, date, description, category, type FROM transactions WHERE account_id = :account_id");
    query.bindValue(":account_id", accountId);

    QList<Transaction> transactions;
    if (query.exec()) {
        while (query.next()) {
            transactions.append(Transaction(
                query.value("id").toInt(),
                query.value("amount").toDouble(),
                QDate::fromString(query.value("date").toString(), "yyyy-MM-dd"),
                query.value("description").toString(),
                query.value("category").toString(),
                query.value("type").toString()
                ));
        }
    } else {
        handleError(query);
    }
    return transactions;
}

void TransactionDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
