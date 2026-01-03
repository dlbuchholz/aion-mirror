#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QString>
#include <QDate>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

class Transaction {
public:
    // Default constructor
    Transaction();

    // Parameterized constructor
    Transaction(int id, int accountId, const QString& payee, double amount, const QDate& date,
                const QString& description, int categoryId, const QString& type);

    // Getters
    int getTransactionId() const;
    int getAccountId() const;
    int getCategoryId() const;
    double getAmount() const;
    QDate getDate() const;
    QString getDescription() const;
    QString getPayee() const;
    QString getType() const;

    // Setters
    void setTransactionId(int id);
    void setAccountId(int id);
    void setCategoryId(int id);
    void setDate(const QDate& date);
    void setDescription(const QString& desc);
    void setPayee(const QString& payee);
    void setAmount(double amt);
    void setType(const QString& transactionType);

    QString calculateHash() const;

private:
    int transactionId;
    int accountId;
    int categoryId;
    double amount;
    QDate date;
    QString description;
    QString payee;
    QString type;
};

// Transaction DAO
class TransactionDAO {
public:
    TransactionDAO();

    int addTransaction(const Transaction& transaction);
    bool updateTransaction(const Transaction& transaction);
    bool deleteTransaction(int transactionId);
    QList<Transaction> getTransactionsForAccount(int accountId);
    QList<Transaction> getShadowedTransactions();
    bool hasShadowedTransactions();
    Transaction getTransactionById(int transactionId);

    bool transactionExistsByHash(const QString& hash);
    QList<QString> getExistingHashes(const QList<QString>& hashes);

private:
    void handleError(const QSqlQuery& query);
};

#endif // TRANSACTION_H
