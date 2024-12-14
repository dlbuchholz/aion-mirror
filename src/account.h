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

    QList<QString> getAccountTypes();

    static int createAccount(int userId, const QString &name, const QString &type, double initialBalance);
    Account getAccountById(int accountId);
    QList<Account> getAllAccountsForUser(int userId);
    bool deleteAccount(int accountId);
private:
    void handleError(const QSqlQuery& query);
};

#endif // ACCOUNT_H
