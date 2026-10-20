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
    Account(int id, const QString& name, const QString& type, Cents balanceCents);

    int getAccountId() const;
    QString getName() const;
    QString getType() const;
    Cents getBalanceCents() const;

private:
    int accountId;
    QString name;
    QString type; // "savings", "checking", etc.
    Cents balanceCents;
};

// Account DAO
class AccountDAO {
public:
    AccountDAO();

    // Account Type Management. Account types belong to a user.
    static bool addDefaultAccountTypes(int userId);  // The types a new user starts with
    QList<QString> getAccountTypes(int userId);
    int addAccountType(int userId, const QString& name);
    bool editAccountType(int userId, int id, const QString& newName);
    bool deleteAccountType(int userId, int id);

    // Account Management
    static int createAccount(int userId, const QString &name, const QString &type, Cents initialBalanceCents);
    Account getAccountById(int accountId);
    QList<Account> getAllAccountsForUser(int userId);
    bool deleteAccount(int accountId);
    QList<QPair<QString, QString> > getAllAccountTypes(int userId);   // (id, name)
    static int ownerOf(int accountId);               // The account's user, -1 if there's no such account
private:
    void handleError(const QSqlQuery& query);
};

#endif // ACCOUNT_H
