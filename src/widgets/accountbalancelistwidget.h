#ifndef ACCOUNTBALANCELISTWIDGET_H
#define ACCOUNTBALANCELISTWIDGET_H

#include <QWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include "../../ui/ui_accountbalancelistwidget.h"

class AccountBalanceListWidget : public QWidget {
    Q_OBJECT

public:
    explicit AccountBalanceListWidget(QWidget *parent);

    void loadAccounts();
    void setUserId(int userId);

private:
    Ui::AccountBalanceListWidget ui;
    int userId;
};

#endif // ACCOUNTBALANCELISTWIDGET_H
