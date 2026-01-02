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
    : QWidget(parent), userId(-1), nextTempId(-1) {
    ui.setupUi(this);

    CheckableComboBox* comboBox = ui.checkableComboBox;

    // Initialize count labels as hidden
    ui.visibleCountLabel->setVisible(false);
    ui.selectedCountLabel->setVisible(false);

    // Connect the combo box selection change to reload transactions
    connect(comboBox, &CheckableComboBox::selectionChanged, [this, comboBox]() {
        setAccountIds(comboBox->checkedItemIds());
    });

    // Connect buttons
    connect(ui.newTransactionToolButton, &QToolButton::clicked, this, &TransactionListWidget::addEmptyTransactionRow);
    connect(ui.deleteButton, &QToolButton::clicked, this, &TransactionListWidget::deleteTransaction);
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
    userId = newUserId;
    populateComboBox();
    loadTransactions();
}

void TransactionListWidget::populateComboBox() {
    CheckableComboBox* comboBox = ui.checkableComboBox;
    comboBox->clear();

    if (userId == -1) {
        qDebug() << "TransactionListWidget: Invalid UserID";
        return;
    }

    // Set the permanent display text
    comboBox->setDisplayText("Account Selection");

    AccountDAO accountDao;
    TransactionDAO transactionDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    qDebug() << "populateComboBox: Populating combo box with" << accounts.size() << "accounts for user" << userId;

    // Check if shadowed transactions exist
    bool hasShadowed = transactionDao.hasShadowedTransactions();
    qDebug() << "populateComboBox: Has shadowed transactions:" << hasShadowed;

    // Only add the "Shadowed Transactions" option if they exist
    if (hasShadowed) {
        comboBox->addItemWithId("(Hidden)", SHADOWED_TRANSACTIONS_ID);
        qDebug() << "populateComboBox:  Added shadowed transactions option";
    }

    // Add regular accounts
    for (const Account& account : accounts) {
        comboBox->addItemWithId(account.getName(), account.getAccountId());
    }

    comboBox->updateWidth();

    // Check all regular accounts by default (but NOT the shadowed option)
    comboBox->blockSignals(true);
    int startIndex = hasShadowed ? 1 :  0;
    for (int i = startIndex; i < comboBox->itemCount(); ++i) {
        comboBox->setItemChecked(i, true);
    }
    comboBox->blockSignals(false);

    // Manually trigger the selection changed signal after setup
    emit comboBox->selectionChanged();

    // Check all items by default
    //comboBox->checkAllItems();
}

void TransactionListWidget::addEmptyTransactionRow() {
    ui.transactionTableWidget->blockSignals(true);

    int newRow = ui.transactionTableWidget->rowCount();
    ui.transactionTableWidget->insertRow(newRow);

    // Use unique temporary ID
    int tempId = nextTempId--;

    // Set temporary ID in vertical header
    QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString::number(tempId));
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
    categoryCombo->addItem("(No Category)", 0);
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

    updateWidget();

    ui.transactionTableWidget->blockSignals(false);

    // Call stageTransactionChange at least once to properly initialize the staged transaction
    // with all the default widget values
    stageTransactionChange(newRow);
}

void TransactionListWidget::deleteTransaction() {
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

            if (transactionId > 0) {
                stagedTransactionDeletions.insert(transactionId);
            }
            stagedTransactions.remove(transactionId);

            ui.transactionTableWidget->removeRow(row);
        }
    }

    // Clear the checked rows set after deletion
    checkedRows.clear();

    updateWidget();

    ui.transactionTableWidget->blockSignals(false);
}

void TransactionListWidget::handleTransactionCheckboxStateChange(int row, bool checked) {
    if (checked) {
        checkedRows.insert(row);
    } else {
        checkedRows.remove(row);
    }

    // Update UI states when checkbox selection changes
    updateWidget();
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


    if (!dateEdit || !accountCombo || !payeeEdit || !descEdit || !categoryCombo || !amountSpin || !typeCombo) {
        qDebug() << "stageTransactionChange: Invalid widget at row" << row;
        // Check which widgets are null
        if (!dateEdit) qDebug() << "Row" << row << ": dateEdit is NULL";
        if (!accountCombo) qDebug() << "Row" << row << ": accountCombo is NULL";
        if (!payeeEdit) qDebug() << "Row" << row << ": payeeEdit is NULL";
        if (!descEdit) qDebug() << "Row" << row << ": descEdit is NULL";
        if (!categoryCombo) qDebug() << "Row" << row << ": categoryCombo is NULL";
        if (!amountSpin) qDebug() << "Row" << row << ": amountSpin is NULL";
        if (!typeCombo) qDebug() << "Row" << row << ": typeCombo is NULL";
        return;
    }

    Transaction transaction;
    transaction.setTransactionId(transactionId);
    transaction.setDate(dateEdit->date());
    transaction.setAccountId(accountCombo->currentData().toInt());
    transaction.setPayee(payeeEdit->text());
    transaction.setDescription(descEdit->text());
    transaction.setCategoryId(categoryCombo->currentData().toInt());
    transaction.setAmount(amountSpin->value());
    transaction.setType(typeCombo->currentText());

    qDebug() << "Staged transaction:" << transactionId
             << "Account:" << transaction.getAccountId()
             << "Payee:" << transaction. getPayee()
             << "Desc:" << transaction.getDescription()
             << "Category:" << transaction.getCategoryId()
             << "Amount:" << transaction.getAmount()
             << "Type:" << transaction.getType();

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

    // Check if shadowed transactions should be shown
    bool showShadowed = selectedAccountIds.contains(SHADOWED_TRANSACTIONS_ID);

    if (showShadowed) {
        qDebug() << "Loading shadowed transactions";
        QList<Transaction> shadowedTransactions = transactionDao.getShadowedTransactions();
        qDebug() << "Found" << shadowedTransactions.size() << "shadowed transactions";

        for (const Transaction& transaction : shadowedTransactions) {
            qDebug() << "Loading shadowed transaction ID:" << transaction.getTransactionId();

            ui.transactionTableWidget->insertRow(row);

            // Set ID in vertical header
            QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString:: number(transaction.getTransactionId()));
            ui.transactionTableWidget->setVerticalHeaderItem(row, idHeaderItem);

            // === Date ===
            QDateEdit* dateEdit = new QDateEdit(transaction.getDate());
            dateEdit->setDisplayFormat("yyyy-MM-dd");
            dateEdit->setCalendarPopup(true);
            connect(dateEdit, &QDateEdit:: dateChanged, [this, row]() {
                stageTransactionChange(row);
            });
            ui.transactionTableWidget->setCellWidget(row, 0, dateEdit);

            // === Account ===
            QComboBox* accountCombo = new QComboBox();

            // Add a special "Shadowed" option first
            accountCombo->addItem("(Hidden)", SHADOWED_TRANSACTIONS_ID);

            // Add all regular accounts
            QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
            for (const Account& acc : accounts) {
                accountCombo->addItem(acc. getName(), acc.getAccountId());
            }

            // Set to "Shadowed" option by default (index 0)
            accountCombo->setCurrentIndex(0);

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
            categoryCombo->addItem("(No Category)", 0);
            QList<Category> categories = categoryDao.getAllCategories();
            for (const Category& cat : categories) {
                categoryCombo->addItem(cat.getName(), cat.getCategoryId());
            }

            int categoryId = transaction.getCategoryId();
            if (categoryId <= 0) {
                categoryCombo->setCurrentIndex(0);
            } else {
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

    // Load regular transactions
    for (int accountId : selectedAccountIds) {

        // Skip the shadowed transactions ID
            if (accountId == SHADOWED_TRANSACTIONS_ID) {
            continue;
        }

        QList<Transaction> transactions = transactionDao.getTransactionsForAccount(accountId);
        Account account = accountDao.getAccountById(accountId);

        qDebug() << "loadTransactions: Found" << transactions.length() << "transactions for account id" << accountId;

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
            categoryCombo->addItem("(No Category)", 0);  // Add "No Category" option with ID 0
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

    updateCountLabels();
}

void TransactionListWidget::saveChanges() {
    TransactionDAO transactionDao;

    bool hasChanges = false;

    // Save staged transaction changes
    for (auto it = stagedTransactions.begin(); it != stagedTransactions. end(); ++it) {
        Transaction transaction = it.value();

        // FIX: In the future we might want to be able to "shadow" transactions by inserting an id <= 0
        if (transaction.getAccountId() <= 0) {
            qWarning() << "ERROR: Transaction has invalid account_id:" << transaction.getAccountId();
            qWarning() << "Skipping this transaction.  Please select a valid account.";
            QMessageBox::warning(this, "Invalid Account",
                                 "One or more transactions have an invalid account.  Please select a valid account for all transactions.");
            continue;  // Skip this transaction
        }

        if (transaction.getTransactionId() < 0) {
            // New transaction
            int newId = transactionDao.addTransaction(transaction);
            if (newId > 0) {
                qDebug() << "Added new Transaction to DB with ID:" << newId;
                hasChanges = true;
            } else {
                qWarning() << "Failed to add new Transaction to DB";
            }
        } else {
            // Existing transaction
            if (!transactionDao.updateTransaction(transaction)) {
                qWarning() << "Failed to update Transaction with ID:" << transaction.getTransactionId();
            } else {
                qDebug() << "Deleted Transaction with ID:" << transaction.getTransactionId();
                hasChanges = true;
            }
        }
    }

    // Handle deletions
    for (int transactionId : stagedTransactionDeletions) {
        if (!transactionDao.deleteTransaction(transactionId)) {
            qWarning() << "Failed to delete Transaction with ID:" << transactionId;
        } else {
            qDebug() << "Deleted Transaction with ID:" << transactionId;
            hasChanges = true;
        }
    }

    // Clear staging
    stagedTransactions.clear();
    stagedTransactionDeletions.clear();
    checkedRows.clear();

    // Reset the temp ID counter for next batch of new transactions
    nextTempId = -1;

    // Refresh the combo box (shadowed option might need to be removed)
    populateComboBox();

    loadTransactions();
    updateWidget();

    // Emit signal if changes were saved
    if (hasChanges) {
        qDebug() << "Emitting transactionsChanged signal";
        emit transactionsChanged();
    }
}

void TransactionListWidget::discardChanges() {
    stagedTransactions.clear();
    stagedTransactionDeletions.clear();
    checkedRows.clear();

    // Reset the temp ID counter
    nextTempId = -1;

    loadTransactions();
    updateWidget();
}

void TransactionListWidget::updateButtonStates() {
    bool hasChanges = !(stagedTransactions.isEmpty() && stagedTransactionDeletions.isEmpty());
    ui.saveButton->setVisible(hasChanges);
    ui.discardButton->setVisible(hasChanges);

    bool hasCheckedRows = !checkedRows.isEmpty();
    ui.deleteButton->setVisible(hasCheckedRows);  // toolButton_2 is the delete button
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

void TransactionListWidget:: updateCountLabels() {
    int visibleCount = ui.transactionTableWidget->rowCount();
    int selectedCount = checkedRows.size();

    // Update visible count label
    if (visibleCount > 0) {
        ui.visibleCountLabel->setText(QString("%1 visible").arg(visibleCount));
        ui.visibleCountLabel->setVisible(true);
    } else {
        ui.visibleCountLabel->setVisible(false);
    }

    // Update selected count label
    if (selectedCount > 0) {
        ui.selectedCountLabel->setText(QString("%1 selected").arg(selectedCount));
        ui.selectedCountLabel->setVisible(true);
        ui.countSeparatorLine->setVisible(true);
    } else {
        ui.selectedCountLabel->setVisible(false);
        ui.countSeparatorLine->setVisible(false);
    }
}

void TransactionListWidget::updateWidget() {
    // Update all UI elements in the correct order
    updateButtonStates();
    updateRowVisualState();
    updateCountLabels();
}
