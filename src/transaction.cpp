#include "db/databasemanager.h"
#include "transaction.h"

// Default constructor
Transaction::Transaction()
    : transactionId(-1), accountId(-1), categoryId(-1), amountCents(0),
    date(QDate::currentDate()), description(""), payee(""), type("expense") {}

// Parameterized constructor
Transaction::Transaction(int id, int accountId, const QString& payee, Cents amountCents, const QDate& date,
                         const QString& description, int categoryId, const QString& type)
    : transactionId(id), accountId(accountId), categoryId(categoryId), amountCents(amountCents),
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

Cents Transaction::getAmountCents() const {
    return amountCents;
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

QString Transaction::getOriginalPayee() const {
    return originalPayee;
}

int Transaction::getRenamedByRuleId() const {
    return renamedByRuleId;
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

void Transaction::setOriginalPayee(const QString& p) {
    originalPayee = p;
}

void Transaction::setRenamedByRuleId(int ruleId) {
    renamedByRuleId = ruleId;
}

void Transaction::renamePayee(const QString& newPayee, int ruleId) {
    if (newPayee == payee) {
        return;
    }
    if (originalPayee.isEmpty()) {
        originalPayee = payee;                       // The first name is the bank's; later renames keep it
    }
    payee = newPayee;
    renamedByRuleId = ruleId;
}

void Transaction::restoreOriginalPayee() {
    if (!originalPayee.isEmpty()) {
        payee = originalPayee;
    }
    originalPayee.clear();
    renamedByRuleId = -1;
}

void Transaction::setAmountCents(Cents cents) {
    amountCents = cents;
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
                             .arg(Money::toPlainString(amountCents))  // Same text as the old 2-decimal format, so hashes stay stable
                             .arg(type. toLower().trimmed())
                             .arg((originalPayee.isEmpty() ? payee : originalPayee).toLower().trimmed())  // The bank's payee
                             .arg(description.toLower().trimmed()); // Normalized description

    // Calculate SHA-256 hash
    QByteArray hashBytes = QCryptographicHash::hash(
        hashSource.toUtf8(),
        QCryptographicHash::Sha256
        );

    // Convert to hex string
    QString hash = QString(hashBytes.toHex());

    return hash;
}

// TransactionDAO Implementation
TransactionDAO::TransactionDAO() {}

// The transaction in the query's current row (id, amount, date, description, payee,
// original_payee, renamed_by_rule, category_id, type)
Transaction TransactionDAO::fromRow(const QSqlQuery& query, int accountId) {
    const int categoryId = query.value("category_id").isNull() ? 0 : query.value("category_id").toInt();
    Transaction transaction(query.value("id").toInt(), accountId, query.value("payee").toString(),
                            query.value("amount").toLongLong(),
                            QDate::fromString(query.value("date").toString(), "yyyy-MM-dd"),
                            query.value("description").toString(), categoryId, query.value("type").toString());
    transaction.setOriginalPayee(query.value("original_payee").toString());
    transaction.setRenamedByRuleId(query.value("renamed_by_rule").isNull() ? -1 : query.value("renamed_by_rule").toInt());
    return transaction;
}

namespace {

QVariant ruleIdValue(const Transaction& transaction) {
    return transaction.getRenamedByRuleId() > 0 ? QVariant(transaction.getRenamedByRuleId())
                                                : QVariant(QMetaType(QMetaType::Int));  // NULL
}

}

int TransactionDAO::addTransaction(const Transaction& transaction) {
    if (transaction.getAmountCents() < 0) {
        qWarning() << "Cannot add a transaction with a negative amount: the type carries the sign";
        return -1;
    }
    QSqlQuery query(DatabaseManager::instance().database());

    // Calculate hash
    QString hash = transaction.calculateHash();

    // Check for duplicate
    /*if (transactionExistsByHash(hash)) {
        qWarning() << "Duplicate transaction detected (hash already exists):" << hash;
        return -1;  // Indicate duplicate
    }*/

    query.prepare(
        "INSERT INTO transactions (account_id, amount, date, description, payee, original_payee, renamed_by_rule, category_id, type, hash) "
        "VALUES (:account_id, :amount, :date, :description, :payee, :original_payee, :renamed_by_rule, :category_id, :type, :hash)"
        );

    query.bindValue(":account_id", transaction.getAccountId());
    query.bindValue(":amount", transaction.getAmountCents());
    query.bindValue(":date", transaction. getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":payee", transaction.getPayee());
    query.bindValue(":original_payee", transaction.getOriginalPayee().isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                                               : QVariant(transaction.getOriginalPayee()));
    query.bindValue(":renamed_by_rule", ruleIdValue(transaction));
    query.bindValue(":hash", hash);

    // Handle NULL category_id
    if (transaction.getCategoryId() <= 0) {
        query.bindValue(":category_id", QVariant(QMetaType(QMetaType::Int)));  // NULL
    } else {
        query.bindValue(":category_id", transaction.getCategoryId());
    }

    query. bindValue(":type", transaction.getType());

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
    if (transaction.getTransactionId() <= 0) {
        qWarning() << "Cannot update transaction with invalid ID:"
                   << transaction.getTransactionId();
        return false;
    }
    if (transaction.getAmountCents() < 0) {
        qWarning() << "Cannot save a transaction with a negative amount: the type carries the sign";
        return false;
    }

    // The hash stays as it was when the transaction was added: it stands for the row the bank sent,
    // so an import still recognises it after the payee or description was changed here. Only a move
    // to another account changes it, since imports look for it under the account they import into.
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "UPDATE transactions SET hash = CASE WHEN account_id = :same_account THEN hash ELSE :moved_hash END, "
        "account_id = :account_id, amount = :amount, date = :date, "
        "description = :description, payee = :payee, original_payee = :original_payee, renamed_by_rule = :renamed_by_rule, "
        "category_id = :category_id, type = :type "
        "WHERE id = :id"
        );
    query.bindValue(":same_account", transaction.getAccountId());
    query.bindValue(":moved_hash", transaction.calculateHash());
    query.bindValue(":id", transaction.getTransactionId());
    query.bindValue(":account_id", transaction.getAccountId());
    query.bindValue(":amount", transaction.getAmountCents());
    query.bindValue(":date", transaction.getDate().toString("yyyy-MM-dd"));
    query.bindValue(":description", transaction.getDescription());
    query.bindValue(":payee", transaction.getPayee());
    query.bindValue(":original_payee", transaction.getOriginalPayee().isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                                               : QVariant(transaction.getOriginalPayee()));
    query.bindValue(":renamed_by_rule", ruleIdValue(transaction));

    // Handle NULL category_id
    if (transaction.getCategoryId() <= 0) {
        query.bindValue(":category_id", QVariant(QMetaType(QMetaType::Int)));  // NULL
    } else {
        query.bindValue(":category_id", transaction.getCategoryId());
    }

    query.bindValue(":type", transaction.getType());

    if (!query.exec()) {
        handleError(query);
        return false;
    }

    if (query.numRowsAffected() != 1) {
        qWarning() << "No transaction found for update:"
                   << transaction.getTransactionId();
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

    const bool deleted = query.numRowsAffected() == 1;
    if (deleted) {
        qDebug() << "Successfully deleted transaction with ID:" << transactionId;
    }
    return deleted;
}

bool TransactionDAO::updateTransactions(const QList<Transaction>& transactions) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    for (const Transaction& transaction : transactions) {
        if (!updateTransaction(transaction)) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool TransactionDAO::deleteTransactions(const QList<int>& transactionIds) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    for (int transactionId : transactionIds) {
        if (!deleteTransaction(transactionId)) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

QList<Transaction> TransactionDAO::getTransactionsForAccount(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, amount, date, description, payee, original_payee, renamed_by_rule, category_id, type FROM transactions WHERE account_id = :account_id");
    query.bindValue(":account_id", accountId);

    QList<Transaction> transactions;
    if (query.exec()) {
        while (query.next()) {
            transactions.append(fromRow(query, accountId));
        }
    } else {
        handleError(query);
    }
    return transactions;
}

QList<Transaction> TransactionDAO:: getShadowedTransactions() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, original_payee, renamed_by_rule, category_id, type FROM transactions WHERE account_id <= 0 OR account_id IS NULL");

    qDebug() << "=== getShadowedTransactions called ===";

    QList<Transaction> transactions;
    if (query.exec()) {
        int count = 0;
        while (query. next()) {
            count++;
            int accountId = query.value("account_id").isNull() ? 0 : query.value("account_id").toInt();

            qDebug() << "  Found shadowed transaction ID:" << query.value("id").toInt()
                     << "with account_id:" << accountId;

            transactions.append(fromRow(query, accountId));
        }
        qDebug() << "  Total shadowed transactions found:" << count;
    } else {
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
        handleError(query);
        return false;
    }
}

Transaction TransactionDAO::getTransactionById(int transactionId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, original_payee, renamed_by_rule, category_id, type FROM transactions WHERE id = :id");
    query.bindValue(":id", transactionId);

    if (query.exec() && query.next()) {
        return fromRow(query, query.value("account_id").toInt());
    } else {
        handleError(query);
        return Transaction();  // Return empty transaction on error
    }
}

QList<Transaction> TransactionDAO::getTransactionsRenamedByRule(int ruleId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, account_id, amount, date, description, payee, original_payee, renamed_by_rule, category_id, type "
                  "FROM transactions WHERE renamed_by_rule = :rule ORDER BY date DESC, id DESC");
    query.bindValue(":rule", ruleId);

    QList<Transaction> transactions;
    if (query.exec()) {
        while (query.next()) {
            transactions.append(fromRow(query, query.value("account_id").toInt()));
        }
    } else {
        handleError(query);
    }
    return transactions;
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

    // In batches: SQLite limits the placeholders of one query, and a failed check would make every
    // row of a large file look new
    constexpr qsizetype BatchSize = 500;
    QSet<QString> existingHashes;
    for (qsizetype start = 0; start < hashes.size(); start += BatchSize) {
        const QList<QString> batch = hashes.mid(start, BatchSize);

        // Build IN clause with placeholders
        QStringList placeholders;
        for (qsizetype i = 0; i < batch.size(); ++i) {
            placeholders << "?";
        }

        QString sql = QString("SELECT DISTINCT hash FROM transactions WHERE hash IN (%1)")
                          .arg(placeholders.join(", "));

        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare(sql);

        // Bind values
        for (const QString& hash : batch) {
            query.addBindValue(hash);
        }

        if (query.exec()) {
            while (query.next()) {
                existingHashes.insert(query.value("hash").toString());
            }
        } else {
            handleError(query);
        }
    }

    return QList<QString>(existingHashes.begin(), existingHashes.end());
}

void TransactionDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        // Not the bound values: they're the user's transactions
        qWarning() << "TransactionDAO:" << query.lastError().text() << query.lastQuery();
    }
}
