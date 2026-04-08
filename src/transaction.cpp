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

QString Transaction::calculateHash() const
{
    // Create a unique string from key fields
    // Using:  account_id + date + amount + type + payee + description (normalized)
    QString hashSource = QString("%1|%2|%3|%4|%5|%6")
                             .arg(accountId)
                             .arg(date.toString(Qt::ISODate))
                             .arg(amount, 0, 'f', 2)  // Fixed precision to avoid floating point issues
                             .arg(type. toLower().trimmed())
                             .arg(payee.toLower().trimmed())  // Normalized payee
                             .arg(description.toLower().trimmed()); // Normalized description

    // Calculate SHA-256 hash
    QByteArray hashBytes = QCryptographicHash::hash(
        hashSource.toUtf8(),
        QCryptographicHash::Sha256
        );

    // Convert to hex string
    QString hash = QString(hashBytes.toHex());

    qDebug() << "Calculated hash for transaction:" << hashSource << "-> " << hash. left(16) << "...";

    return hash;
}

// TransactionDAO Implementation
TransactionDAO::TransactionDAO() {}

int TransactionDAO::addTransaction(const Transaction& transaction) {
    QSqlQuery query(DatabaseManager::instance().database());

    // Calculate hash
    QString hash = transaction.calculateHash();

    // Check for duplicate
    /*if (transactionExistsByHash(hash)) {
        qWarning() << "Duplicate transaction detected (hash already exists):" << hash;
        return -1;  // Indicate duplicate
    }*/

    query.prepare(
        "INSERT INTO transactions (account_id, amount, date, description, payee, category_id, type, hash) "
        "VALUES (:account_id, :amount, :date, :description, :payee, :category_id, :type, :hash)"
        );

    query.bindValue(":account_id", transaction.getAccountId());
    query.bindValue(":amount", transaction.getAmount());
    query.bindValue(":date", transaction. getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":payee", transaction.getPayee());
    query.bindValue(":hash", hash);

    // Handle NULL category_id
    if (transaction.getCategoryId() <= 0) {
        query.bindValue(":category_id", QVariant(QVariant::Int));  // NULL
    } else {
        query.bindValue(":category_id", transaction.getCategoryId());
    }

    query. bindValue(":type", transaction. getType());

    if (query.exec()) {
        int newId = query.lastInsertId().toInt();
        qDebug() << "Transaction added with ID:" << newId << "Hash:" << hash.left(16) << "...";
        return newId;
    } else {
        handleError(query);
        return -1;
    }
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
            int categoryId = query.value("category_id").isNull() ? 0 : query.value("category_id").toInt();

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

QList<Transaction> TransactionDAO:: getShadowedTransactions() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, category_id, type FROM transactions WHERE account_id <= 0 OR account_id IS NULL");

    qDebug() << "=== getShadowedTransactions called ===";

    QList<Transaction> transactions;
    if (query.exec()) {
        int count = 0;
        while (query. next()) {
            count++;
            int categoryId = query.value("category_id").isNull() ? 0 : query.value("category_id").toInt();
            int accountId = query.value("account_id").isNull() ? 0 : query.value("account_id").toInt();

            qDebug() << "  Found shadowed transaction ID:" << query.value("id").toInt()
                     << "with account_id:" << accountId;

            transactions.append(Transaction(
                query.value("id").toInt(),
                accountId,
                query.value("payee").toString(),
                query.value("amount").toDouble(),
                QDate::fromString(query.value("date").toString(), "yyyy-MM-dd"),
                query.value("description").toString(),
                categoryId,
                query.value("type").toString()
                ));
        }
        qDebug() << "  Total shadowed transactions found:" << count;
    } else {
        qDebug() << "  Query execution failed! ";
        qDebug() << "  SQL Error:" << query.lastError().text();
        handleError(query);
    }

    return transactions;
}

bool TransactionDAO::hasShadowedTransactions() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COUNT(*) as count FROM transactions WHERE account_id <= 0 OR account_id IS NULL");

    if (query.exec() && query.next()) {
        int count = query.value("count").toInt();
        qDebug() << "hasShadowedTransactions:  Found" << count << "shadowed transactions";
        return count > 0;
    } else {
        qDebug() << "hasShadowedTransactions: Query failed";
        handleError(query);
        return false;
    }
}

Transaction TransactionDAO::getTransactionById(int transactionId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, category_id, type FROM transactions WHERE id = :id");
    query.bindValue(":id", transactionId);

    if (query.exec() && query.next()) {
        int categoryId = query.value("category_id").isNull() ? 0 : query.value("category_id").toInt();

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

bool TransactionDAO::transactionExistsByHash(const QString& hash)
{
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COUNT(*) as count FROM transactions WHERE hash = :hash");
    query.bindValue(":hash", hash);

    if (query.exec() && query.next()) {
        int count = query.value("count").toInt();
        return count > 0;
    }

    handleError(query);
    return false;
}

QList<QString> TransactionDAO::getExistingHashes(const QList<QString>& hashes)
{
    if (hashes.isEmpty()) {
        return QList<QString>();
    }

    // Build IN clause with placeholders
    QStringList placeholders;
    for (int i = 0; i < hashes.size(); ++i) {
        placeholders << "?";
    }

    QString sql = QString("SELECT DISTINCT hash FROM transactions WHERE hash IN (%1)")
                      .arg(placeholders.join(", "));

    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(sql);

    // Bind values
    for (const QString& hash : hashes) {
        query.addBindValue(hash);
    }

    QList<QString> existingHashes;

    if (query.exec()) {
        while (query.next()) {
            existingHashes. append(query.value("hash").toString());
        }
    } else {
        handleError(query);
    }

    return existingHashes;
}

void TransactionDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "TransactionDAO / Failed query:" << query.lastQuery();
        qDebug() << "TransactionDAO / Bound values:" << query.boundValues();
        qDebug() << "TransactionDAO / SQL Error:" << query.lastError().text();
    }
}
