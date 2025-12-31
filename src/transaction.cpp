#include "db/databasemanager.h"
#include "transaction.h"

// Default constructor
Transaction::Transaction()
    : transactionId(-1), accountId(-1), payee(""), categoryId(-1), amount(0.0),
    date(QDate::currentDate()), description(""), type("expense") {}

// Parameterized constructor
Transaction::Transaction(int id, int accountId, const QString& payee, double amount, const QDate& date,
                         const QString& description, int categoryId, const QString& type)
    : transactionId(id), accountId(accountId), categoryId(categoryId), amount(amount),
    date(date), description(description), payee(payee), type(type) {}

// Getters
int Transaction::getTransactionId() const {
    return transactionId;
}

int Transaction::getAccountId() const {
    return accountId;
}

int Transaction:: getCategoryId() const {
    return categoryId;
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

QString Transaction::getPayee() const {
    return payee;
}

QString Transaction:: getType() const {
    return type;
}

// Setters
void Transaction::setTransactionId(int id) {
    transactionId = id;
}

void Transaction::setAccountId(int id) {
    accountId = id;
}

void Transaction::setCategoryId(int id) {
    categoryId = id;
}

void Transaction::setDate(const QDate& transactionDate) {
    date = transactionDate;
}

void Transaction::setDescription(const QString& desc) {
    description = desc;
}

void Transaction::setPayee(const QString& p) {
    payee = p;
}

void Transaction::setAmount(double amt) {
    amount = amt;
}

void Transaction::setType(const QString& transactionType) {
    type = transactionType;
}

// TransactionDAO Implementation
TransactionDAO::TransactionDAO() {}

int TransactionDAO::addTransaction(const Transaction& transaction) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "INSERT INTO transactions (account_id, amount, date, description, payee, category_id, type) "
        "VALUES (:account_id, :amount, :date, :description, :payee, :category_id, :type)"
        );
    query.bindValue(":account_id", transaction.getAccountId());
    query.bindValue(":amount", transaction.getAmount());
    query.bindValue(":date", transaction. getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":payee", transaction.getPayee());

    // Handle NULL category_id
    if (transaction.getCategoryId() <= 0) {
        query.bindValue(":category_id", QVariant(QVariant::Int));  // NULL
    } else {
        query.bindValue(":category_id", transaction.getCategoryId());
    }

    query. bindValue(":type", transaction. getType());

    if (!query.exec()) {
        qDebug() << "SQL Error:" << query.lastError().text();
        qDebug() << "Last query:" << query.lastQuery();
        qDebug() << "Bound values:" << query. boundValues();
        handleError(query);
        return -1;
    }

    int newId = query.lastInsertId().toInt();
    qDebug() << "Successfully added transaction with ID:" << newId;
    return newId;
}

bool TransactionDAO::updateTransaction(const Transaction& transaction) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "UPDATE transactions SET account_id = :account_id, amount = :amount, date = :date, "
        "description = :description, payee = :payee, category_id = :category_id, type = :type WHERE id = :id"
        );
    query.bindValue(":id", transaction.getTransactionId());
    query.bindValue(":account_id", transaction.getAccountId());
    query.bindValue(":amount", transaction.getAmount());
    query.bindValue(":date", transaction.getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":payee", transaction.getPayee());

    // Handle NULL category_id
    if (transaction.getCategoryId() <= 0) {
        query.bindValue(":category_id", QVariant(QVariant::Int));  // NULL
    } else {
        query.bindValue(":category_id", transaction.getCategoryId());
    }

    query.bindValue(":type", transaction.getType());

    if (!query.exec()) {
        qDebug() << "SQL Error:" << query.lastError().text();
        handleError(query);
        return false;
    }

    qDebug() << "Successfully updated transaction with ID:" << transaction.getTransactionId();
    return true;
}

bool TransactionDAO::deleteTransaction(int transactionId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM transactions WHERE id = :id");
    query.bindValue(":id", transactionId);

    if (!query.exec()) {
        handleError(query);
        return false;
    }

    qDebug() << "Successfully deleted transaction with ID:" << transactionId;
    return true;
}

QList<Transaction> TransactionDAO::getTransactionsForAccount(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, amount, date, description, payee, category_id, type FROM transactions WHERE account_id = :account_id");
    query.bindValue(":account_id", accountId);

    QList<Transaction> transactions;
    if (query.exec()) {
        while (query.next()) {
            int categoryId = query.value("category_id").isNull() ? -1 : query.value("category_id").toInt();

            transactions.append(Transaction(
                query.value("id").toInt(),
                accountId,
                query.value("payee").toString(),
                query.value("amount").toDouble(),
                QDate::fromString(query.value("date").toString(), "yyyy-MM-dd"),
                query.value("description").toString(),
                categoryId,
                query. value("type").toString()
                ));
        }
    } else {
        handleError(query);
    }
    return transactions;
}

Transaction TransactionDAO::getTransactionById(int transactionId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, category_id, type FROM transactions WHERE id = :id");
    query.bindValue(":id", transactionId);

    if (query.exec() && query.next()) {
        int categoryId = query.value("category_id").isNull() ? -1 : query.value("category_id").toInt();

        return Transaction(
            query.value("id").toInt(),
            query.value("account_id").toInt(),
            query.value("payee").toString(),
            query.value("amount").toDouble(),
            QDate::fromString(query.value("date").toString(), "yyyy-MM-dd"),
            query.value("description").toString(),
            categoryId,
            query.value("type").toString()
            );
    } else {
        handleError(query);
        return Transaction();  // Return empty transaction on error
    }
}

void TransactionDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
