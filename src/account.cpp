#include "db/databasemanager.h"
#include "account.h"

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

// AccountDAO Implementation
AccountDAO::AccountDAO() {}

QList<QString> AccountDAO::getAccountTypes() {
    QList<QString> accountTypes;
    QSqlQuery query(DatabaseManager::instance().database());

    if (query.exec("SELECT name FROM account_types")) {
        while (query.next()) {
            accountTypes.append(query.value(0).toString());
        }
    } else {
        qDebug() << "Failed to fetch account types:" << query.lastError().text();
    }
    return accountTypes;
}

QList<QPair<QString, QString>> AccountDAO::getAllAccountTypes() {
    QList<QPair<QString, QString>> accountTypes;
    QSqlQuery query(DatabaseManager::instance().database());

    if (query.exec("SELECT id, name FROM account_types")) {
        while (query.next()) {
            QString id = query.value(0).toString();
            QString name = query.value(1).toString();
            accountTypes.append(qMakePair(id, name));
        }
    } else {
        qDebug() << "Failed to fetch account types:" << query.lastError().text();
    }
    return accountTypes;
}

int AccountDAO::addAccountType(const QString& name) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO account_types (name) VALUES (:name)");
    query.bindValue(":name", name);

    if (!query.exec()) {
        qWarning() << "Failed to add account type:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool AccountDAO::editAccountType(int id, const QString& newName) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("UPDATE account_types SET name = :name WHERE id = :id");
    query.bindValue(":name", newName);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to edit account type:" << query.lastError().text();
        return false;
    }

    return true;
}

bool AccountDAO::deleteAccountType(int id) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM account_types WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to delete account type:" << query.lastError().text();
        return false;
    }

    return true;
}

int AccountDAO::createAccount(int userId, const QString& name, const QString& accountTypeName, double initialBalance) {
    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    // Start a transaction
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction:" << db.lastError().text();
        return -1;
    }

    // Retrieve the account type ID
    query.prepare("SELECT id FROM account_types WHERE name = :name");
    query.bindValue(":name", accountTypeName);
    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to fetch account type ID for:" << accountTypeName << query.lastError().text();
        db.rollback();
        return -1;
    }
    int accountTypeId = query.value("id").toInt();

    // Insert the new account
    query.prepare("INSERT INTO accounts (user_id, name, type_id) VALUES (:user_id, :name, :type_id)");
    query.bindValue(":user_id", userId);
    query.bindValue(":name", name);
    query.bindValue(":type_id", accountTypeId);

    if (!query.exec()) {
        qWarning() << "Failed to insert account:" << query.lastError().text();
        db.rollback();
        return -1;
    }

    // Get the inserted account ID
    int accountId = query.lastInsertId().toInt();

    // Insert an initial transaction if the balance is non-zero
    if (initialBalance != 0.0) {
        Transaction initialTransaction(-1, accountId, "", initialBalance, QDate::currentDate(), "Initial deposit", 0, "income");
        TransactionDAO transactionDao;
        if (!transactionDao.addTransaction(initialTransaction)) {
            qWarning() << "Failed to insert initial transaction";
            db.rollback();
            return -1;
        }
    }

    // Commit the transaction
    if (!db.commit()) {
        qWarning() << "Failed to commit transaction:" << db.lastError().text();
        return -1;
    }

    return accountId;
}

Account AccountDAO::getAccountById(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT account_id, account_name, account_type, balance
        FROM account_balances
        WHERE account_id = :id
    )");
    query.bindValue(":id", accountId);

    if (query.exec() && query.next()) {
        return Account(
            query.value("account_id").toInt(),
            query.value("account_name").toString(),
            query.value("account_type").toString(),
            query.value("balance").toDouble()
            );
    }

    handleError(query);
    return Account(-1, "", "", 0.0); // Invalid account on failure
}

QList<Account> AccountDAO::getAllAccountsForUser(int userId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(R"(
        SELECT account_id, account_name, account_type, balance
        FROM account_balances
        WHERE user_id = :user_id
    )");
    query.bindValue(":user_id", userId);

    QList<Account> accounts;
    if (query.exec()) {
        while (query.next()) {
            accounts.append(Account(
                query.value("account_id").toInt(),
                query.value("account_name").toString(),
                query.value("account_type").toString(),
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
        qDebug() << "AccountDAO / Failed query:" << query.lastQuery();
        qDebug() << "AccountDAO / Bound values:" << query.boundValues();
        qDebug() << "AccountDAO / SQL Error:" << query.lastError().text();
    }
}
