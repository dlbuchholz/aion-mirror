#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QString>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// Transaction Object
class Transaction {
public:
    Transaction(int id, double amount, const QDate& date, const QString& description, const QString& category, const QString& type);

    int getTransactionId() const;
    double getAmount() const;
    QDate getDate() const;
    QString getDescription() const;
    QString getCategory() const;
    QString getType() const;

private:
    int transactionId;
    double amount;
    QDate date;
    QString description;
    QString category;
    QString type;
};

// Transaction DAO
class TransactionDAO {
public:
    TransactionDAO();

    bool addTransaction(const Transaction& transaction);
    QList<Transaction> getTransactionsForAccount(int accountId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // TRANSACTION_H
