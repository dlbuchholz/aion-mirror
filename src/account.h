#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "transaction.h"

// Account Object
class Account {
public:
    Account(int id, const QString& name, const QString& type, double balance);

    int getAccountId() const;
    QString getName() const;
    QString getType() const;
    double getBalance() const;

    void updateBalance(double amount);

private:
    int accountId;
    QString name;
    QString type; // "savings", "checking", etc.
    double balance;
};

// Account DAO
class AccountDAO {
public:
    AccountDAO();

    bool createAccount(const Account& account);
    Account getAccountById(int accountId);
    QList<Account> getAllAccounts();
    bool deleteAccount(int accountId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // ACCOUNT_H
