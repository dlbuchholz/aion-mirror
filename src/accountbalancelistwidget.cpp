#include "accountbalancelistwidget.h"
#include "db/databasemanager.h"

AccountBalanceListWidget::AccountBalanceListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    // Initialize the account table
    ui.accountTableWidget->setColumnCount(2);
    ui.accountTableWidget->setHorizontalHeaderLabels({"Type", "Balance"});
    ui.accountTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void AccountBalanceListWidget::setUserId(int newUserId) {
    if (userId != newUserId) {
        userId = newUserId;
        loadAccounts(); // Reload accounts for the new user
    }
}

void AccountBalanceListWidget::loadAccounts() {
    QSqlQuery query(DatabaseManager::instance().database());
    qDebug() << "Loading accounts for user ID:" << userId;

    query.prepare("SELECT name, type, balance FROM accounts WHERE user_id = :user_id");
    query.bindValue(":user_id", userId);

    if (!query.exec()) {
        QMessageBox::critical(this, "Error", "Failed to load accounts: " + query.lastError().text());
        return;
    }

    if (!query.next()) {
        qDebug() << "No accounts found for user ID:" << userId;
        return;
    }

    ui.accountTableWidget->setRowCount(0); // Clear existing rows

    int row = 0;
    while (query.next()) {
        QString accountName = query.value("name").toString();
        QString accountType = query.value("type").toString();
        double balance = query.value("balance").toDouble();

        ui.accountTableWidget->insertRow(row);
        ui.accountTableWidget->setVerticalHeaderItem(row, new QTableWidgetItem(accountName));
        ui.accountTableWidget->setItem(row, 1, new QTableWidgetItem(accountType));
        ui.accountTableWidget->setItem(row, 0, new QTableWidgetItem(QString::number(balance, 'f', 2)));

        row++;
    }
}
