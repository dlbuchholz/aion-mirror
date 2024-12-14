#include "accountbalancelistwidget.h"
#include "account.h"

AccountBalanceListWidget::AccountBalanceListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    // Initialize the account table
    ui.accountTableWidget->setColumnCount(2);
    ui.accountTableWidget->setHorizontalHeaderLabels({"Type", "Balance"});
    ui.accountTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui.addAccountWidget, &AddAccountWidget::accountCreated, this, &AccountBalanceListWidget::loadAccounts);
}

void AccountBalanceListWidget::setUserId(int newUserId) {
    if (userId != newUserId) {
        userId = newUserId;
        ui.addAccountWidget->setUserId(userId);
        loadAccounts(); // Reload accounts for the new user
    }
}

void AccountBalanceListWidget::loadAccounts() {
    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    ui.accountTableWidget->setRowCount(0); // Clear existing rows

    int row = 0;
    for (const Account& account : accounts) {
        ui.accountTableWidget->insertRow(row);

        // Set vertical header as account name
        ui.accountTableWidget->setVerticalHeaderItem(row, new QTableWidgetItem(account.getName()));

        // Set account type in the first column
        ui.accountTableWidget->setItem(row, 0, new QTableWidgetItem(account.getType()));

        // Set calculated balance in the second column
        ui.accountTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(account.getBalance(), 'f', 2)));

        row++;
    }

    qDebug() << "Accounts loaded successfully!";
}
