#include "db/databasemanager.h"
#include "Account.h"

// Account Implementation
Account::Account(int id, const QString& name, const QString& type, double balance)
    : accountId(id), name(name), type(type), balance(balance) {}

int Account::getAccountId() const {
    return accountId;
}

QString Account::getName() const {
    return name;
}

QString Account::getType() const {
    return type;
}

double Account::getBalance() const {
    return balance;
}

void Account::updateBalance(double amount) {
    balance += amount;
}

// AccountDAO Implementation
AccountDAO::AccountDAO() {}

bool AccountDAO::createAccount(const Account& account) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO accounts (id, name, type, balance) VALUES (:id, :name, :type, :balance)");
    query.bindValue(":id", account.getAccountId());
    query.bindValue(":name", account.getName());
    query.bindValue(":type", account.getType());
    query.bindValue(":balance", account.getBalance());

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

Account AccountDAO::getAccountById(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, name, type, balance FROM accounts WHERE id = :id");
    query.bindValue(":id", accountId);

    if (query.exec() && query.next()) {
        return Account(
            query.value("id").toInt(),
            query.value("name").toString(),
            query.value("type").toString(),
            query.value("balance").toDouble()
            );
    }

    handleError(query);
    return Account(-1, "", "", 0.0); // Invalid account on failure
}

QList<Account> AccountDAO::getAllAccounts() {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, name, type, balance FROM accounts");

    QList<Account> accounts;
    if (query.exec()) {
        while (query.next()) {
            accounts.append(Account(
                query.value("id").toInt(),
                query.value("name").toString(),
                query.value("type").toString(),
                query.value("balance").toDouble()
                ));
        }
    } else {
        handleError(query);
    }
    return accounts;
}

bool AccountDAO::deleteAccount(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM accounts WHERE id = :id");
    query.bindValue(":id", accountId);

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

void AccountDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
