#include "TransactionListWidget.h"
#include "transaction.h"
#include "account.h"

#include <QDateEdit>
#include <QDoubleSpinBox>

TransactionListWidget::TransactionListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    CheckableComboBox* comboBox = ui.checkableComboBox;

    // Connect the combo box selection change to reload transactions
    connect(comboBox, &CheckableComboBox::selectionChanged, [this, comboBox]() {
        setAccountIds(comboBox->checkedItemIds());
    });
    connect(ui.newTransactionToolButton, &QToolButton::clicked, this, &TransactionListWidget::addEmptyTransactionRow);

    // Configure the table widget
    ui.transactionTableWidget->setColumnCount(7); // Adjust column count based on fields
    ui.transactionTableWidget->setHorizontalHeaderLabels({"Date", "Account", "Payee", "Description", "Category", "Amount", "Type"});
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.transactionTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui.transactionTableWidget->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    //ui.transactionTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
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

void TransactionListWidget::addEmptyTransactionRow() {
    int newRow = ui.transactionTableWidget->rowCount();
    ui.transactionTableWidget->insertRow(newRow);

    // === Date Picker ===
    QDateEdit* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setCalendarPopup(true);
    ui.transactionTableWidget->setCellWidget(newRow, 0, dateEdit);

    // === Account ComboBox ===
    QComboBox* accountCombo = new QComboBox();
    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
    for (const Account& acc : accounts) {
        accountCombo->addItem(acc.getName(), acc.getAccountId());
    }
    ui.transactionTableWidget->setCellWidget(newRow, 1, accountCombo);

    // === Payee (plain text) ===
    QLineEdit* payeeEdit = new QLineEdit();
    ui.transactionTableWidget->setCellWidget(newRow, 2, payeeEdit);

    // === Description (plain text) ===
    QLineEdit* descEdit = new QLineEdit();
    ui.transactionTableWidget->setCellWidget(newRow, 3, descEdit);

    // === Category ComboBox ===
    QComboBox* categoryCombo = new QComboBox();
    categoryCombo->addItems({"Salary", "Groceries", "Rent", "Entertainment"});
    ui.transactionTableWidget->setCellWidget(newRow, 4, categoryCombo);

    // === Amount SpinBox ===
    QDoubleSpinBox* amountSpin = new QDoubleSpinBox();
    amountSpin->setPrefix("");
    amountSpin->setMaximum(1'000'000);
    amountSpin->setDecimals(2);
    amountSpin->setValue(0.00);
    ui.transactionTableWidget->setCellWidget(newRow, 5, amountSpin);

    // === Type ComboBox ===
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"Expense", "Income"});
    ui.transactionTableWidget->setCellWidget(newRow, 6, typeCombo);
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
