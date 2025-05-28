#include "TransactionListWidget.h"
#include "transaction.h"
#include "account.h"

TransactionListWidget::TransactionListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    CheckableComboBox* comboBox = ui.checkableComboBox;

    // Connect the combo box selection change to reload transactions
    connect(comboBox, &CheckableComboBox::selectionChanged, [this, comboBox]() {
        setAccountIds(comboBox->checkedItemIds());
    });

    // Configure the table widget
    ui.transactionTableWidget->setColumnCount(7); // Adjust column count based on fields
    ui.transactionTableWidget->setHorizontalHeaderLabels({"Date", "Account", "Payee", "Description", "Category", "Amount", "Type"});
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.transactionTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.transactionTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void TransactionListWidget::setUserId(int newUserId) {
    if (userId != newUserId) {
        userId = newUserId;
        populateComboBox();
        loadTransactions();
    }
}

void TransactionListWidget::populateComboBox() {
    CheckableComboBox* comboBox = ui.checkableComboBox;
    comboBox->clear(); // Clear existing items in the combo box

    if (userId == -1) {
        qDebug() << "TransactionListWidget: Invalid UserID";
        return; // Do nothing if the user ID is invalid
    }

    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    for (const Account& account : accounts) {
        comboBox->addItemWithId(account.getName(), account.getAccountId());
    }
}

void TransactionListWidget::setAccountIds(const QSet<int>& accountIds) {
    selectedAccountIds = accountIds;
    loadTransactions(); // Reload transactions based on the new selection
}

void TransactionListWidget::loadTransactions() {
    if (selectedAccountIds.isEmpty()) {
        ui.transactionTableWidget->setRowCount(0); // Clear table if no accounts are selected
        return;
    }

    TransactionDAO transactionDao;
    AccountDAO accountDao;

    // Clear existing rows
    ui.transactionTableWidget->setRowCount(0);

    int row = 0;

    for (int accountId : selectedAccountIds) {
        // Fetch transactions for the current account
        QList<Transaction> transactions = transactionDao.getTransactionsForAccount(accountId);

        // Fetch account details
        Account account = accountDao.getAccountById(accountId);

        for (const Transaction& transaction : transactions) {
            ui.transactionTableWidget->insertRow(row);

            // Populate row data
            ui.transactionTableWidget->setItem(row, 0, new QTableWidgetItem(transaction.getDate().toString("yyyy-MM-dd")));
            ui.transactionTableWidget->setItem(row, 1, new QTableWidgetItem(account.getName()));
            ui.transactionTableWidget->setItem(row, 2, new QTableWidgetItem("")); // Placeholder for Payee
            ui.transactionTableWidget->setItem(row, 3, new QTableWidgetItem(transaction.getDescription()));
            ui.transactionTableWidget->setItem(row, 4, new QTableWidgetItem(transaction.getCategory()));
            ui.transactionTableWidget->setItem(row, 5, new QTableWidgetItem(QString::number(transaction.getAmount(), 'f', 2)));
            ui.transactionTableWidget->setItem(row, 6, new QTableWidgetItem(transaction.getType()));

            row++;
        }
    }
}
