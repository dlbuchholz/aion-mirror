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

    // Account Type Management
    QList<QString> getAccountTypes();
    int addAccountType(const QString& name);
    bool editAccountType(int id, const QString& newName);
    bool deleteAccountType(int id);

    // Account Management
    static int createAccount(int userId, const QString &name, const QString &type, double initialBalance);
    Account getAccountById(int accountId);
    QList<Account> getAllAccountsForUser(int userId);
    bool deleteAccount(int accountId);
    QList<QPair<QString, QString> > getAllAccountTypes();
private:
    void handleError(const QSqlQuery& query);
};

#endif // ACCOUNT_H
