#include "db/databasemanager.h"
#include "user.h"

// User Implementation
User::User(int id, const QString& username, const QString& name)
    : userId(id), username(username), name(name) {}

int User::getUserId() const {
    return userId;
}

QString User::getUsername() const {
    return username;
}

QString User::getName() const {
    return name;
}

// UserDAO Implementation
UserDAO::UserDAO() {}

bool UserDAO::createUser(const User& user) {
    QSqlQuery query(DatabaseManager::instance().database());
    if (user.getUserId() > 0) {
        query.prepare("INSERT INTO users (id, username, name) VALUES (:id, :username, :name)");
        query.bindValue(":id", user.getUserId());
    } else {
        query.prepare("INSERT INTO users (username, name) VALUES (:username, :name)");
    }
    query.bindValue(":username", user.getUsername());
    query.bindValue(":name", user.getName());

    if (!query.exec()) {
        handleError(query);
        return false;
    }
    return true;
}

User UserDAO::getUserById(int userId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id, username, name FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (query.exec() && query.next()) {
        return User(query.value("id").toInt(), query.value("username").toString(), query.value("name").toString());
    }

    handleError(query);
    return User(-1, "", ""); // Invalid user on failure
}

QList<User> UserDAO::getAllUsers() {
    QSqlQuery query(DatabaseManager::instance().database());
    QList<User> users;
    if (!query.exec("SELECT id, username, name FROM users ORDER BY name COLLATE NOCASE")) {
        handleError(query);
        return users;
    }
    while (query.next()) {
        users.append(User(query.value("id").toInt(), query.value("username").toString(), query.value("name").toString()));
    }
    return users;
}

int UserDAO::findUserIdByUsername(const QString& username) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT id FROM users WHERE username = :username");
    query.bindValue(":username", username);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    handleError(query);
    return -1;
}

void UserDAO::handleError(const QSqlQuery& query) {
    if (query.lastError().isValid()) {
        qDebug() << "SQL Error:" << query.lastError().text();
    }
}
