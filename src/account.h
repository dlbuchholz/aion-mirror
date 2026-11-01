#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QDate>
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
    // Balances count what is dated up to today; later payments are still to come
    Account getAccountById(int accountId, const QDate& today = QDate::currentDate());
    QList<Account> getAllAccountsForUser(int userId, const QDate& today = QDate::currentDate());
    bool editAccount(int accountId, const QString& newName, int typeId);
    bool isNameTaken(int userId, const QString& name, int exceptAccountId = -1);   // Ignores case
    // Deletes the account and its transactions. Rules stop naming it: one that needs the account is
    // deleted, as without that condition it would match more than before.
    bool deleteAccount(int accountId);
    // Moves its transactions and rules to another account of the same user first
    bool deleteAccountMovingTo(int accountId, int targetAccountId);
    int transactionCount(int accountId);
    int ruleCount(int accountId);                    // Rules with a condition on this account
    QList<QPair<QString, QString> > getAllAccountTypes(int userId);   // (id, name)
    static int ownerOf(int accountId);               // The account's user, -1 if there's no such account
private:
    void handleError(const QSqlQuery& query);
};

#endif // ACCOUNT_H
