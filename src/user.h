#ifndef USER_H
#define USER_H

#include <QString>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// User Object
class User {
public:
    User(int id, const QString& username, const QString& name);

    int getUserId() const;
    QString getUsername() const;
    QString getName() const;

private:
    int userId;
    QString username;
    QString name;
};

// User DAO
class UserDAO {
public:
    UserDAO();

    bool createUser(const User& user);
    User getUserById(int userId);

private:
    void handleError(const QSqlQuery& query);
};

#endif // USER_H
