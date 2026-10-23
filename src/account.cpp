#include "db/databasemanager.h"
#include "account.h"
#include "category.h"

// Account Implementation
Account::Account(int id, const QString& name, const QString& type, Cents balanceCents)
    : accountId(id), name(name), type(type), balanceCents(balanceCents) {}

int Account::getAccountId() const {
    return accountId;
}

QString Account::getName() const {
    return name;
}

QString Account::getType() const {
    return type;
}

Cents Account::getBalanceCents() const {
    return balanceCents;
}

// AccountDAO Implementation
AccountDAO::AccountDAO() {}

bool AccountDAO::addDefaultAccountTypes(int userId) {
    AccountDAO dao;
    const QStringList defaults = {"Checking", "Savings", "Investment", "Retirement", "Virtual"};
    for (const QString& name : defaults) {
        if (dao.addAccountType(userId, name) <= 0) {
            return false;
        }
    }
    return true;
}

QList<QString> AccountDAO::getAccountTypes(int userId) {
    QList<QString> accountTypes;
    for (const auto& type : getAllAccountTypes(userId)) {
        accountTypes.append(type.second);
    }
    return accountTypes;
}

QList<QPair<QString, QString>> AccountDAO::getAllAccountTypes(int userId) {
    QList<QPair<QString, QString>> accountTypes;
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, name FROM account_types WHERE user_id = :user_id ORDER BY id");
    query.bindValue(":user_id", userId);

    if (query.exec()) {
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

int AccountDAO::addAccountType(int userId, const QString& name) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("INSERT INTO account_types (user_id, name) VALUES (:user_id, :name)");
    query.bindValue(":user_id", userId);
    query.bindValue(":name", name);

    if (!query.exec()) {
        qWarning() << "Failed to add account type:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool AccountDAO::editAccountType(int userId, int id, const QString& newName) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("UPDATE account_types SET name = :name WHERE id = :id AND user_id = :user_id");
    query.bindValue(":name", newName);
    query.bindValue(":id", id);
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        qWarning() << "Failed to edit account type:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool AccountDAO::deleteAccountType(int userId, int id) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM account_types WHERE id = :id AND user_id = :user_id");
    query.bindValue(":id", id);
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        qWarning() << "Failed to delete account type:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

int AccountDAO::ownerOf(int accountId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT user_id FROM accounts WHERE id = :id");
    query.bindValue(":id", accountId);
    return query.exec() && query.next() ? query.value(0).toInt() : -1;
}

int AccountDAO::createAccount(int userId, const QString& name, const QString& accountTypeName, Cents initialBalanceCents) {
    const QString trimmedName = name.trimmed();
    const QString trimmedType = accountTypeName.trimmed();

    if (userId <= 0) {
        qWarning() << "Invalid userId:" << userId;
        return -1;
    }
    if (trimmedName.isEmpty()) {
        qWarning() << "Account name is empty";
        return -1;
    }
    if (trimmedType.isEmpty()) {
        qWarning() << "Account type is empty";
        return -1;
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    QSqlQuery query(db);

    // Start a transaction
    if (!db.transaction()) {
        qWarning() << "Failed to start transaction:" << db.lastError().text();
        return -1;
    }

    // Retrieve the account type ID
    query.prepare("SELECT id FROM account_types WHERE name = :name AND user_id = :user_id");
    query.bindValue(":user_id", userId);
    query.bindValue(":name", trimmedType);
    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to fetch account type ID for:" << trimmedType << query.lastError().text();
        db.rollback();
        return -1;
    }
    int accountTypeId = query.value("id").toInt();

    // Insert the new account
    query.prepare("INSERT INTO accounts (user_id, name, type_id) VALUES (:user_id, :name, :type_id)");
    query.bindValue(":user_id", userId);
    query.bindValue(":name", trimmedName);
    query.bindValue(":type_id", accountTypeId);

    if (!query.exec()) {
        qWarning() << "Failed to insert account:" << query.lastError().text();
        db.rollback();
        return -1;
    }

    // Get the inserted account ID
    int accountId = query.lastInsertId().toInt();

    // Insert an initial transaction if the balance is non-zero (amounts are positive; the type carries the sign)
    if (initialBalanceCents != 0) {
        // In the starting balance category if there is one, so it isn't counted as income or spending
        int categoryId = 0;
        query.prepare("SELECT id FROM categories WHERE name = :name AND user_id = :user_id");
        query.bindValue(":user_id", userId);
        query.bindValue(":name", InitialBalanceCategory);
        if (query.exec() && query.next()) {
            categoryId = query.value(0).toInt();
        }
        Transaction initialTransaction(-1, accountId, "", qAbs(initialBalanceCents), QDate::currentDate(), "Initial deposit", categoryId,
                                       initialBalanceCents > 0 ? "income" : "expense");
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
            query.value("balance").toLongLong()
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
                query.value("balance").toLongLong()
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
    return query.numRowsAffected() > 0;
}

void AccountDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "AccountDAO / Failed query:" << query.lastQuery();
        qDebug() << "AccountDAO / Bound values:" << query.boundValues();
        qDebug() << "AccountDAO / SQL Error:" << query.lastError().text();
    }
}
