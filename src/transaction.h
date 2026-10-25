#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QString>
#include <QDate>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "money.h"

class Transaction {
public:
    // Default constructor
    Transaction();

    // Parameterized constructor
    Transaction(int id, int accountId, const QString& payee, Cents amountCents, const QDate& date,
                const QString& description, int categoryId, const QString& type);

    // Getters
    int getTransactionId() const;
    int getAccountId() const;
    int getCategoryId() const;
    Cents getAmountCents() const;                    // Always positive; the type says income or expense
    QDate getDate() const;
    QString getDescription() const;
    QString getPayee() const;
    QString getOriginalPayee() const;                // Before a rule renamed it; empty = never renamed
    QString getType() const;

    // Setters
    void setTransactionId(int id);
    void setAccountId(int id);
    void setCategoryId(int id);
    void setDate(const QDate& date);
    void setDescription(const QString& desc);
    void setPayee(const QString& payee);
    void setOriginalPayee(const QString& payee);
    // Renames the payee and keeps the first original (the bank's) in originalPayee
    void renamePayee(const QString& newPayee);
    void setAmountCents(Cents cents);
    void setType(const QString& transactionType);

    // For duplicate detection: built from the bank's data, so the original payee if renamed
    QString calculateHash() const;

private:
    int transactionId;
    int accountId;
    int categoryId;
    Cents amountCents;
    QDate date;
    QString description;
    QString payee;
    QString type;
    QString originalPayee;
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
    static Transaction fromRow(const QSqlQuery& query, int accountId);
};

#endif // TRANSACTION_H
