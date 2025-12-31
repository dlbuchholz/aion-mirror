#include "TransactionListWidget.h"
#include "transaction.h"
#include "account.h"
#include "category.h"

#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <algorithm>

TransactionListWidget::TransactionListWidget(QWidget* parent)
    : QWidget(parent), userId(-1) {
    ui.setupUi(this);

    CheckableComboBox* comboBox = ui.checkableComboBox;

    // Connect the combo box selection change to reload transactions
    connect(comboBox, &CheckableComboBox::selectionChanged, [this, comboBox]() {
        setAccountIds(comboBox->checkedItemIds());
    });

    // Connect buttons
    connect(ui.newTransactionToolButton, &QToolButton::clicked, this, &TransactionListWidget::addEmptyTransactionRow);
    connect(ui.toolButton_2, &QToolButton::clicked, this, &TransactionListWidget::deleteTransaction);
    connect(ui.saveButton, &QPushButton::clicked, this, &TransactionListWidget::saveChanges);
    connect(ui.discardButton, &QPushButton::clicked, this, &TransactionListWidget::discardChanges);

    // Configure the table widget
    ui.transactionTableWidget->setColumnCount(8); // Added checkbox column
    ui.transactionTableWidget->setHorizontalHeaderLabels({"Date", "Account", "Payee", "Description", "Category", "Amount", "Type", "Select"});

    // Set column resize modes
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Date
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView:: Stretch);          // Account
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);          // Payee
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);          // Description
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);          // Category
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Amount
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents); // Type
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(7, QHeaderView:: ResizeToContents); // Checkbox
    ui.transactionTableWidget->setColumnWidth(7, 30);  // Small width for checkbox

    ui.transactionTableWidget->verticalHeader()->setVisible(true);  // Show vertical header for IDs
    ui.transactionTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.transactionTableWidget->setEditTriggers(QAbstractItemView:: DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    // Initial button state
    updateButtonStates();
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
    comboBox->clear();

    if (userId == -1) {
        qDebug() << "TransactionListWidget: Invalid UserID";
        return;
    }

    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    for (const Account& account : accounts) {
        comboBox->addItemWithId(account.getName(), account.getAccountId());
    }

    // Check all items by default
    comboBox->checkAllItems();
}

void TransactionListWidget::addEmptyTransactionRow() {
    ui.transactionTableWidget->blockSignals(true);

    int newRow = ui.transactionTableWidget->rowCount();
    ui.transactionTableWidget->insertRow(newRow);

    // Set temporary ID in vertical header
    QTableWidgetItem *idHeaderItem = new QTableWidgetItem("-1");
    ui.transactionTableWidget->setVerticalHeaderItem(newRow, idHeaderItem);

    // === Date Picker ===
    QDateEdit* dateEdit = new QDateEdit(QDate::currentDate());
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setCalendarPopup(true);
    connect(dateEdit, &QDateEdit::dateChanged, [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 0, dateEdit);

    // === Account ComboBox ===
    QComboBox* accountCombo = new QComboBox();
    AccountDAO accountDao;
    QList<Account> accounts = accountDao. getAllAccountsForUser(userId);
    for (const Account& acc : accounts) {
        accountCombo->addItem(acc.getName(), acc.getAccountId());
    }
    connect(accountCombo, QOverload<int>:: of(&QComboBox::currentIndexChanged), [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 1, accountCombo);

    // === Payee (plain text) ===
    QLineEdit* payeeEdit = new QLineEdit();
    connect(payeeEdit, &QLineEdit:: textChanged, [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 2, payeeEdit);

    // === Description (plain text) ===
    QLineEdit* descEdit = new QLineEdit();
    connect(descEdit, &QLineEdit::textChanged, [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 3, descEdit);

    // === Category ComboBox ===
    QComboBox* categoryCombo = new QComboBox();
    CategoryDAO categoryDao;
    QList<Category> categories = categoryDao. getAllCategories();
    for (const Category& cat : categories) {
        categoryCombo->addItem(cat.getName(), cat.getCategoryId());
    }
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 4, categoryCombo);

    // === Amount SpinBox ===
    QDoubleSpinBox* amountSpin = new QDoubleSpinBox();
    amountSpin->setPrefix("");
    amountSpin->setMaximum(1'000'000);
    amountSpin->setDecimals(2);
    amountSpin->setValue(0.00);
    connect(amountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 5, amountSpin);

    // === Type ComboBox ===
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"expense", "income"});
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, newRow]() {
        stageTransactionChange(newRow);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 6, typeCombo);

    // === Checkbox ===
    QCheckBox *checkBox = new QCheckBox();
    connect(checkBox, &QCheckBox::clicked, [=]() {
        handleTransactionCheckboxStateChange(newRow, checkBox->isChecked());
    });
    QWidget *checkBoxWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
    layout->addWidget(checkBox);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    ui.transactionTableWidget->setCellWidget(newRow, 7, checkBoxWidget);

    // Stage the new transaction
    Transaction newTransaction;
    newTransaction. setTransactionId(-1);
    newTransaction.setDate(QDate::currentDate());
    newTransaction.setAmount(0.00);
    newTransaction.setType("expense");
    stagedTransactions[-1] = newTransaction;

    updateButtonStates();
    updateRowVisualState();

    ui.transactionTableWidget->blockSignals(false);
}

void TransactionListWidget:: deleteTransaction() {
    if (checkedRows.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select transactions to delete by checking their checkboxes.");
        return;
    }

    ui.transactionTableWidget->blockSignals(true);

    // Process rows from highest to lowest to avoid index shifting issues
    QList<int> sortedRows = checkedRows.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (int row : sortedRows) {
        if (row >= 0 && row < ui.transactionTableWidget->rowCount()) {
            QString id = ui.transactionTableWidget->verticalHeaderItem(row)->text();
            int transactionId = id.toInt();

            if (transactionId != -1) {
                stagedTransactionDeletions. insert(transactionId);
            }
            stagedTransactions. remove(transactionId);

            ui.transactionTableWidget->removeRow(row);
        }
    }

    // Clear the checked rows set after deletion
    checkedRows.clear();

    updateButtonStates();
    updateRowVisualState();

    ui.transactionTableWidget->blockSignals(false);
}

void TransactionListWidget::handleTransactionCheckboxStateChange(int row, bool checked) {
    if (checked) {
        checkedRows.insert(row);
    } else {
        checkedRows.remove(row);
    }
}

void TransactionListWidget:: stageTransactionChange(int row) {
    QString id = ui.transactionTableWidget->verticalHeaderItem(row)->text();
    int transactionId = id.toInt();

    // Extract data from widgets
    QDateEdit* dateEdit = qobject_cast<QDateEdit*>(ui.transactionTableWidget->cellWidget(row, 0));
    QComboBox* accountCombo = qobject_cast<QComboBox*>(ui.transactionTableWidget->cellWidget(row, 1));
    QLineEdit* payeeEdit = qobject_cast<QLineEdit*>(ui.transactionTableWidget->cellWidget(row, 2));
    QLineEdit* descEdit = qobject_cast<QLineEdit*>(ui.transactionTableWidget->cellWidget(row, 3));
    QComboBox* categoryCombo = qobject_cast<QComboBox*>(ui.transactionTableWidget->cellWidget(row, 4));
    QDoubleSpinBox* amountSpin = qobject_cast<QDoubleSpinBox*>(ui.transactionTableWidget->cellWidget(row, 5));
    QComboBox* typeCombo = qobject_cast<QComboBox*>(ui.transactionTableWidget->cellWidget(row, 6));

    if (! dateEdit || !accountCombo || !payeeEdit || !descEdit || !categoryCombo || !amountSpin || !typeCombo) {
        qDebug() << "stageTransactionChange: Invalid widget at row" << row;
        return;
    }

    Transaction transaction;
    transaction.setTransactionId(transactionId);
    transaction.setDate(dateEdit->date());
    transaction.setAccountId(accountCombo->currentData().toInt());
    transaction.setPayee(payeeEdit->text());  // ADD THIS
    transaction.setDescription(descEdit->text());
    transaction.setCategoryId(categoryCombo->currentData().toInt());
    transaction.setAmount(amountSpin->value());
    transaction.setType(typeCombo->currentText());

    stagedTransactions[transactionId] = transaction;
    updateButtonStates();
    updateRowVisualState();
}

void TransactionListWidget::setAccountIds(const QSet<int>& accountIds) {
    selectedAccountIds = accountIds;
    loadTransactions();
}

void TransactionListWidget::loadTransactions() {
    qDebug() << "loadTransactions called with selectedAccountIds:" << selectedAccountIds;

    ui.transactionTableWidget->blockSignals(true);

    if (selectedAccountIds.isEmpty()) {
        qDebug() << "No accounts selected, clearing table";
        ui. transactionTableWidget->setRowCount(0);
        ui.transactionTableWidget->blockSignals(false);
        checkedRows.clear();
        return;
    }

    TransactionDAO transactionDao;
    AccountDAO accountDao;
    CategoryDAO categoryDao;

    ui.transactionTableWidget->setRowCount(0);

    int row = 0;

    for (int accountId : selectedAccountIds) {
        QList<Transaction> transactions = transactionDao.getTransactionsForAccount(accountId);
        Account account = accountDao.getAccountById(accountId);

        for (const Transaction& transaction :  transactions) {
            ui.transactionTableWidget->insertRow(row);

            // Set ID in vertical header
            QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString:: number(transaction.getTransactionId()));
            ui.transactionTableWidget->setVerticalHeaderItem(row, idHeaderItem);

            // === Date ===
            QDateEdit* dateEdit = new QDateEdit(transaction.getDate());
            dateEdit->setDisplayFormat("yyyy-MM-dd");
            dateEdit->setCalendarPopup(true);
            connect(dateEdit, &QDateEdit::dateChanged, [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 0, dateEdit);

            // === Account ===
            QComboBox* accountCombo = new QComboBox();
            QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
            for (const Account& acc : accounts) {
                accountCombo->addItem(acc. getName(), acc.getAccountId());
            }
            accountCombo->setCurrentText(account.getName());
            connect(accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 1, accountCombo);

            // === Payee ===
            QLineEdit* payeeEdit = new QLineEdit(transaction.getPayee());
            connect(payeeEdit, &QLineEdit::textChanged, [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 2, payeeEdit);

            // === Description ===
            QLineEdit* descEdit = new QLineEdit(transaction.getDescription());
            connect(descEdit, &QLineEdit::textChanged, [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 3, descEdit);

            // === Category ===
            QComboBox* categoryCombo = new QComboBox();
            categoryCombo->addItem("(No Category)", -1);  // Add "No Category" option with ID -1
            QList<Category> categories = categoryDao.getAllCategories();
            for (const Category& cat : categories) {
                categoryCombo->addItem(cat.getName(), cat.getCategoryId());
            }

            // Set current category
            int categoryId = transaction.getCategoryId();
            if (categoryId <= 0) {
                categoryCombo->setCurrentIndex(0);  // Select "(No Category)"
            } else {
                // Find and set the correct category
                for (int i = 0; i < categoryCombo->count(); ++i) {
                    if (categoryCombo->itemData(i).toInt() == categoryId) {
                        categoryCombo->setCurrentIndex(i);
                        break;
                    }
                }
            }

            connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 4, categoryCombo);

            // === Amount ===
            QDoubleSpinBox* amountSpin = new QDoubleSpinBox();
            amountSpin->setMaximum(1'000'000);
            amountSpin->setDecimals(2);
            amountSpin->setValue(transaction.getAmount());
            connect(amountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 5, amountSpin);

            // === Type ===
            QComboBox* typeCombo = new QComboBox();
            typeCombo->addItems({"expense", "income"});
            typeCombo->setCurrentText(transaction.getType());
            connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 6, typeCombo);

            // === Checkbox ===
            QCheckBox *checkBox = new QCheckBox();
            connect(checkBox, &QCheckBox::clicked, [=]() {
                handleTransactionCheckboxStateChange(row, checkBox->isChecked());
            });
            QWidget *checkBoxWidget = new QWidget();
            QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
            layout->addWidget(checkBox);
            layout->setAlignment(Qt::AlignCenter);
            layout->setContentsMargins(0, 0, 0, 0);
            ui.transactionTableWidget->setCellWidget(row, 7, checkBoxWidget);

            row++;
        }
    }

    ui.transactionTableWidget->blockSignals(false);
    checkedRows.clear();
}

void TransactionListWidget::saveChanges() {
    TransactionDAO transactionDao;

    // Save staged transaction changes
    for (auto it = stagedTransactions.begin(); it != stagedTransactions. end(); ++it) {
        Transaction transaction = it.value();

        if (transaction.getTransactionId() == -1) {
            // New transaction
            int newId = transactionDao.addTransaction(transaction);
            if (newId != -1) {
                qDebug() << "Added new Transaction to DB with ID:" << newId;
            } else {
                qWarning() << "Failed to add new Transaction to DB";
            }
        } else {
            // Existing transaction
            if (! transactionDao.updateTransaction(transaction)) {
                qWarning() << "Failed to update Transaction with ID:" << transaction.getTransactionId();
            }
        }
    }

    // Handle deletions
    for (int transactionId : stagedTransactionDeletions) {
        if (!transactionDao.deleteTransaction(transactionId)) {
            qWarning() << "Failed to delete Transaction with ID:" << transactionId;
        } else {
            qDebug() << "Deleted Transaction with ID:" << transactionId;
        }
    }

    // Clear staging
    stagedTransactions.clear();
    stagedTransactionDeletions.clear();
    checkedRows.clear();

    loadTransactions();
    updateButtonStates();
    updateRowVisualState();
}

void TransactionListWidget::discardChanges() {
    stagedTransactions.clear();
    stagedTransactionDeletions. clear();
    checkedRows.clear();
    loadTransactions();
    updateButtonStates();
    updateRowVisualState();
}

void TransactionListWidget::updateButtonStates() {
    bool hasChanges = !(stagedTransactions.isEmpty() && stagedTransactionDeletions.isEmpty());
    ui.saveButton->setVisible(hasChanges);
    ui.discardButton->setVisible(hasChanges);
}

void TransactionListWidget::updateRowVisualState() {
    /*
    for (int row = 0; row < ui.transactionTableWidget->rowCount(); ++row) {
        QString id = ui.transactionTableWidget->verticalHeaderItem(row)->text();
        int transactionId = id.toInt();

        bool isPending = stagedTransactions.contains(transactionId);
        bool isDeleted = stagedTransactionDeletions.contains(transactionId);

        // Apply visual styling to the description cell (column 3)
        QWidget* descWidget = ui.transactionTableWidget->cellWidget(row, 3);
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(descWidget)) {
            if (isDeleted) {
                lineEdit->setStyleSheet("background-color: #FFC8C8; color: #960000;");
                QFont font = lineEdit->font();
                font.setStrikeOut(true);
                lineEdit->setFont(font);
            } else if (isPending) {
                lineEdit->setStyleSheet("background-color: #FFFFC8;");
                QFont font = lineEdit->font();
                font. setStrikeOut(false);
                lineEdit->setFont(font);
            } else {
                lineEdit->setStyleSheet("");
                QFont font = lineEdit->font();
                font.setStrikeOut(false);
                lineEdit->setFont(font);
            }
        }
    }*/
}
