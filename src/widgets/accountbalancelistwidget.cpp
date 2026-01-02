#include "accountbalancelistwidget.h"
#include "account.h"
#include "settings.h"

AccountBalanceListWidget::AccountBalanceListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    // Initialize the account table
    ui.accountTableWidget->setColumnCount(2);
    ui.accountTableWidget->setHorizontalHeaderLabels({"Type", "Balance"});
    ui.accountTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui.addAccountWidget, &AddAccountWidget::accountCreated, this, [this]() {
        updateWidget();

        // Notify parent (MainWindow) that accounts changed
        emit accountsChanged();
    });

    // Initialize with formatted zero
    ui.totalBalanceLabel->setText(Settings::instance().formatCurrency(0.0));
}

void AccountBalanceListWidget::setUserId(int newUserId) {
    if (userId != newUserId) {
        userId = newUserId;
        ui.addAccountWidget->setUserId(userId);
        updateWidget();
    }
}

void AccountBalanceListWidget::updateWidget()
{
    loadAccounts();
    updateTotalBalance();
}

void AccountBalanceListWidget::updateTotalBalance() {
    if (userId == -1) {
        // No user set, show placeholder with proper formatting
        ui.totalBalanceLabel->setText(Settings::instance().formatCurrency(0.0));
        ui.totalBalanceLabel->setStyleSheet("color: white; font-weight: bold;");
        return;
    }

    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    double totalBalance = 0.0;
    for (const Account& account : accounts) {
        totalBalance += account. getBalance();
    }

    // Format with Settings - showSign = true to display + or -
    QString formattedBalance = Settings::instance().formatCurrency(totalBalance, false);

    // Color code based on positive/negative
    if (totalBalance < 0) {
        ui.totalBalanceLabel->setStyleSheet("color:  #ff4444; font-weight: bold;");  // Red for negative
    } else if (totalBalance > 0) {
        ui.totalBalanceLabel->setStyleSheet("color:  #44ff44; font-weight: bold;");  // Green for positive
    } else {
        ui.totalBalanceLabel->setStyleSheet("color: white; font-weight: bold;");    // White for zero
    }

    ui.totalBalanceLabel->setText(formattedBalance);

    qDebug() << "Updated total balance label:" << formattedBalance;
}

void AccountBalanceListWidget::loadAccounts() {
    if (userId == -1) {
        qDebug() << "AccountBalanceListWidget:  Invalid userId";
        return;
    }

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

        // Set calculated balance using Settings formatting
        double balance = account.getBalance();
        QString formattedBalance = Settings::instance().formatCurrency(balance, false);  // No + sign for individual accounts
        ui.accountTableWidget->setItem(row, 1, new QTableWidgetItem(formattedBalance));

        row++;
    }

    qDebug() << "Accounts loaded successfully!";
}
