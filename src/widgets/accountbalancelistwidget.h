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
    void updateWidget();                             // General function that calls all other update UI functions

signals:
    void accountsChanged();

private:
    void updateTotalBalance();

    Ui::AccountBalanceListWidget ui;
    int userId;
};

#endif // ACCOUNTBALANCELISTWIDGET_H
