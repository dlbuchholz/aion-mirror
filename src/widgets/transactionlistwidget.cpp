#include "transactionlistwidget.h"
#include "transaction.h"
#include "account.h"
#include "category.h"

#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <algorithm>
#include "settings.h"

#include <dialog/importmanagerdialog.h>

TransactionListWidget::TransactionListWidget(QWidget* parent)
    : QWidget(parent),
    userId(-1),
    nextTempId(-1),
    useLazyLoading(Settings::instance().isLazyLoadingEnabled()),
    useDetailEdit(Settings::instance().isDetailEditEnabled()),
    isTableLocked(false),
    currentDetailTransactionId(0),
    currentDetailEditField(DetailEditField::None),
    currentDetailEditWidget(nullptr)
{
    ui.setupUi(this);

    // Initialize lazy loader
    lazyLoader = new LazyTableLoader(ui.transactionTableWidget, this);
    lazyLoader->setEnabled(useLazyLoading);

    // Set up row insert callback
    lazyLoader->setRowInsertCallback([this](int row, const Transaction& transaction, bool isShadowed) {
        insertTransactionRow(row, transaction, isShadowed);
    });

    // Connect lazy loader signals
    connect(lazyLoader, &LazyTableLoader::countChanged, this, [this](int visible, int total) {
        updateCountLabels();
    });

    CheckableComboBox* comboBox = ui.checkableComboBox;

    // Initialize count labels as hidden
    ui.visibleCountLabel->setVisible(false);
    ui.selectedCountLabel->setVisible(false);

    // Setup and connect lock table checkbox
    ui.lockTableCheckBox->setChecked(isTableLocked);
    connect(ui.lockTableCheckBox, &QCheckBox::toggled, this, &TransactionListWidget::onLockTableCheckboxToggled);

    // Hide detail edit frame initially
    ui.detailEditFrame->setVisible(false);
    ui.detailEditCheckBox->setChecked(useDetailEdit);
    connect(ui.detailTextEdit, &QTextEdit::textChanged, this, &TransactionListWidget::onDetailEditTextChanged);
    connect(ui.closeDetailEditButton, &QToolButton::clicked, this, &TransactionListWidget::closeDetailEdit);
    connect(ui.detailEditCheckBox, &QCheckBox::toggled, this, &TransactionListWidget::onDetailEditCheckboxToggled);


    // Connect the combo box selection change to reload transactions
    connect(comboBox, &CheckableComboBox::selectionChanged, [this, comboBox]() {
        setAccountIds(comboBox->checkedItemIds());
    });

    // Connect buttons
    connect(ui.newTransactionToolButton, &QToolButton::clicked, this, &TransactionListWidget::addEmptyTransactionRow);
    connect(ui.deleteButton, &QToolButton::clicked, this, &TransactionListWidget::deleteTransaction);
    connect(ui.saveButton, &QPushButton::clicked, this, &TransactionListWidget::saveChanges);
    connect(ui.discardButton, &QPushButton::clicked, this, &TransactionListWidget::discardChanges);
    connect(ui.importButton, &QAbstractButton::clicked, this, &TransactionListWidget::onImportTransactionsClicked);

    // Configure the table widget
    ui.transactionTableWidget->setColumnCount(8);
    ui.transactionTableWidget->setHorizontalHeaderLabels({"Date", "Account", "Payer/Payee", "Description", "Category", "Amount", "Type", "Select"});

    // Set column resize modes
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Date
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);          // Account
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);          // Payee
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);          // Description
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);          // Category
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Amount
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents); // Type
    ui.transactionTableWidget->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents); // Checkbox
    ui.transactionTableWidget->setColumnWidth(7, 30);  // Small width for checkbox

    ui.transactionTableWidget->verticalHeader()->setVisible(true);  // Show vertical header for IDs
    ui.transactionTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.transactionTableWidget->setEditTriggers(QAbstractItemView:: DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    // Initial button state
    updateButtonStates();
}

void TransactionListWidget::reinitialize() {
    qDebug() << "Reinitializing TransactionListWidget with new settings";

    // Update lazy loading setting
    bool newUseLazyLoading = Settings::instance().isLazyLoadingEnabled();
    bool newUseDetailEdit = Settings::instance().isDetailEditEnabled();

    if (newUseLazyLoading != useLazyLoading) {
        qDebug() << "Lazy loading setting changed from" << useLazyLoading << "to" << newUseLazyLoading;
        useLazyLoading = newUseLazyLoading;

        if (lazyLoader) {
            lazyLoader->setEnabled(useLazyLoading);
        }

        // Reload transactions with new loading strategy
        loadTransactions();
    }

    if (newUseDetailEdit != useDetailEdit) {
        qDebug() << "Detail edit setting changed from" << useDetailEdit << "to" << newUseDetailEdit;
        useDetailEdit = newUseDetailEdit;

        // Close detail edit if it's being disabled
        if (!useDetailEdit) {
            closeDetailEdit();
        }

        // Reload transactions to update widget states
        loadTransactions();
    }
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
    {
        const QSignalBlocker blocker(comboBox);
        int startIndex = hasShadowed ? 1 :  0;
        for (int i = startIndex; i < comboBox->itemCount(); ++i) {
            comboBox->setItemChecked(i, true);
        }
    }

    // Manually trigger the selection changed signal after setup
    emit comboBox->selectionChanged();

    // Check all items by default
    //comboBox->checkAllItems();
}

void TransactionListWidget::showAccounts(const QSet<int>& accountIds) {
    CheckableComboBox* comboBox = ui.checkableComboBox;
    {
        const QSignalBlocker blocker(comboBox);
        for (int i = 0; i < comboBox->itemCount(); ++i) {
            const int id = comboBox->itemId(i);
            const bool checked = accountIds.isEmpty() ? id != SHADOWED_TRANSACTIONS_ID : accountIds.contains(id);
            comboBox->setItemChecked(i, checked);
        }
    }
    emit comboBox->selectionChanged();
}

void TransactionListWidget::addEmptyTransactionRow() {
    QSignalBlocker tableSignals(ui.transactionTableWidget);

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
    connect(dateEdit, &QDateEdit::dateChanged, this, [this, dateEdit]() {
        const int row = rowForWidget(dateEdit);
        if (row >= 0) stageTransactionChange(row);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 0, dateEdit);

    // === Account ComboBox ===
    QComboBox* accountCombo = new QComboBox();
    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
    for (const Account& acc : accounts) {
        accountCombo->addItem(acc.getName(), acc.getAccountId());
    }
    for (int accountId : ui.checkableComboBox->checkedItemIdsInOrder()) {
        if (accountId == SHADOWED_TRANSACTIONS_ID) continue;
        const int index = accountCombo->findData(accountId);
        if (index >= 0) {
            accountCombo->setCurrentIndex(index);
            break;
        }
    }
    connect(accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, accountCombo]() {
        const int row = rowForWidget(accountCombo);
        if (row >= 0) stageTransactionChange(row);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 1, accountCombo);

    // === Payee (plain text) ===
    QLineEdit* payeeEdit = new QLineEdit();
    connect(payeeEdit, &QLineEdit::textChanged, this, [this, payeeEdit]() {
        const int row = rowForWidget(payeeEdit);
        if (row >= 0) stageTransactionChange(row);
    });
    setupDetailEditConnections(payeeEdit, tempId, DetailEditField::Payee);
    ui.transactionTableWidget->setCellWidget(newRow, 2, payeeEdit);

    // === Description (plain text) ===
    QLineEdit* descEdit = new QLineEdit();
    connect(descEdit, &QLineEdit::textChanged, this, [this, descEdit]() {
        const int row = rowForWidget(descEdit);
        if (row >= 0) stageTransactionChange(row);
    });
    setupDetailEditConnections(descEdit, tempId, DetailEditField::Description);
    ui. transactionTableWidget->setCellWidget(newRow, 3, descEdit);

    // === Category ComboBox ===
    QComboBox* categoryCombo = new QComboBox();
    categoryCombo->addItem("(No Category)", 0);
    CategoryDAO categoryDao;
    QList<Category> categories = categoryDao. getAllCategories();
    for (const Category& cat : categories) {
        categoryCombo->addItem(cat.getName(), cat.getCategoryId());
    }
    connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, categoryCombo]() {
        const int row = rowForWidget(categoryCombo);
        if (row >= 0) stageTransactionChange(row);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 4, categoryCombo);

    // === Amount SpinBox ===
    QDoubleSpinBox* amountSpin = new QDoubleSpinBox();
    amountSpin->setPrefix("");
    amountSpin->setMaximum(1'000'000);
    amountSpin->setDecimals(2);
    amountSpin->setValue(0.00);
    connect(amountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, amountSpin]() {
        const int row = rowForWidget(amountSpin);
        if (row >= 0) stageTransactionChange(row);
    });
    setupDetailEditConnections(amountSpin, tempId, DetailEditField::Amount);
    ui.transactionTableWidget->setCellWidget(newRow, 5, amountSpin);

    // === Type ComboBox ===
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"expense", "income"});
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, typeCombo]() {
        const int row = rowForWidget(typeCombo);
        if (row >= 0) stageTransactionChange(row);
    });
    ui.transactionTableWidget->setCellWidget(newRow, 6, typeCombo);

    // === Checkbox ===
    QCheckBox *checkBox = new QCheckBox();
    connect(checkBox, &QCheckBox::clicked, this, [this, checkBox, tempId]() {
        handleTransactionCheckboxStateChange(tempId, checkBox->isChecked());
    });
    QWidget *checkBoxWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
    layout->addWidget(checkBox);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    ui.transactionTableWidget->setCellWidget(newRow, 7, checkBoxWidget);

    updateWidget();

    tableSignals.unblock();

    // Call stageTransactionChange at least once to properly initialize the staged transaction
    // with all the default widget values
    stageTransactionChange(newRow);
}

void TransactionListWidget::deleteTransaction() {
    if (checkedTransactionIds.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select transactions to delete by checking their checkboxes.");
        return;
    }

    QSignalBlocker tableSignals(ui.transactionTableWidget);

    // Process rows from highest to lowest to avoid index shifting issues
    QList<int> sortedRows;
    for (int transactionId : checkedTransactionIds) {
        const int row = rowForTransactionId(transactionId);
        if (row >= 0) sortedRows.append(row);
    }
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
    checkedTransactionIds.clear();

    updateWidget();

    tableSignals.unblock();

    updatePinnedTransactions();
}

void TransactionListWidget::handleTransactionCheckboxStateChange(int transactionId, bool checked) {
    if (checked) {
        checkedTransactionIds.insert(transactionId);
    } else {
        checkedTransactionIds.remove(transactionId);
    }

    // Update UI states when checkbox selection changes
    updateWidget();
    updatePinnedTransactions();
}

void TransactionListWidget::stageTransactionChange(int row) {
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
    updatePinnedTransactions();
}

int TransactionListWidget::transactionIdForRow(int row) const {
    if (row < 0 || row >= ui.transactionTableWidget->rowCount()) return 0;
    const QTableWidgetItem* header = ui.transactionTableWidget->verticalHeaderItem(row);
    return header ? header->text().toInt() : 0;
}

int TransactionListWidget::rowForTransactionId(int transactionId) const {
    for (int row = 0; row < ui.transactionTableWidget->rowCount(); ++row) {
        if (transactionIdForRow(row) == transactionId) return row;
    }
    return -1;
}

int TransactionListWidget::rowForWidget(const QWidget* widget) const {
    if (!widget) return -1;
    const QPoint viewportPoint = widget->mapTo(ui.transactionTableWidget->viewport(), QPoint(1, 1));
    return ui.transactionTableWidget->indexAt(viewportPoint).row();
}

void TransactionListWidget::setAccountIds(const QSet<int>& accountIds) {
    selectedAccountIds = accountIds;
    loadTransactions();
}

void TransactionListWidget::loadTransactions() {
    qDebug() << "loadTransactions called with selectedAccountIds:" << selectedAccountIds;

    // Close detail edit when loading new transactions (with safety check)
    if (currentDetailTransactionId != 0) {
        qDebug() << "Closing detail edit before loading transactions";
        ui.detailEditFrame->setVisible(false);
        currentDetailTransactionId = 0;
        currentDetailEditField = DetailEditField::None;
        currentDetailEditWidget = nullptr;
    }

    if (selectedAccountIds.isEmpty()) {
        qDebug() << "No accounts selected, clearing table";
        {
            const QSignalBlocker blocker(ui.transactionTableWidget);
            ui.transactionTableWidget->setRowCount(0);
        }

        if (lazyLoader) {
            lazyLoader->clear();
        }

        checkedTransactionIds.clear();
        updateCountLabels();
        return;
    }

    TransactionDAO transactionDao;
    QList<Transaction> allTransactions;

    // Check if shadowed transactions should be shown
    bool showShadowed = selectedAccountIds.contains(SHADOWED_TRANSACTIONS_ID);

    if (showShadowed) {
        qDebug() << "Loading shadowed transactions";
        QList<Transaction> shadowedTransactions = transactionDao.getShadowedTransactions();
        qDebug() << "Found" << shadowedTransactions.size() << "shadowed transactions";
        allTransactions. append(shadowedTransactions);
    }

    // Load regular transactions
    for (int accountId : selectedAccountIds) {
        if (accountId == SHADOWED_TRANSACTIONS_ID) {
            continue;
        }

        QList<Transaction> transactions = transactionDao.getTransactionsForAccount(accountId);
        qDebug() << "Found" << transactions.size() << "transactions for account" << accountId;
        allTransactions.append(transactions);
    }

    qDebug() << "Total transactions loaded from DB:" << allTransactions.size();

    // Clear table
    {
        const QSignalBlocker blocker(ui.transactionTableWidget);
        ui.transactionTableWidget->setRowCount(0);
    }
    checkedTransactionIds.clear();

    if (useLazyLoading && lazyLoader) {
        // Use lazy loading
        qDebug() << "Using lazy loading";
        lazyLoader->setAllData(allTransactions);
        lazyLoader->loadInitialBatch();
    } else {
        // Load all at once (legacy behavior)
        qDebug() << "Using legacy loading (all at once)";
        for (const Transaction& transaction : allTransactions) {
            bool isShadowed = (transaction.getAccountId() <= 0);
            int row = ui.transactionTableWidget->rowCount();
            insertTransactionRow(row, transaction, isShadowed);
        }
    }

    updateCountLabels();
}

void TransactionListWidget::saveChanges() {

    // Close detail edit before saving
    if (currentDetailTransactionId != 0) {
        qDebug() << "Closing detail edit before saving changes";
        // Sync one last time before closing
        if (rowForTransactionId(currentDetailTransactionId) >= 0 && currentDetailEditWidget) {
            syncDetailEditToTable();
        }
        ui.detailEditFrame->setVisible(false);
        currentDetailTransactionId = 0;
        currentDetailEditField = DetailEditField:: None;
        currentDetailEditWidget = nullptr;
    }

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
    checkedTransactionIds.clear();

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
    // Close detail edit before discarding
    if (currentDetailTransactionId != 0) {
        qDebug() << "Closing detail edit before discarding changes";
        ui.detailEditFrame->setVisible(false);
        currentDetailTransactionId = 0;
        currentDetailEditField = DetailEditField::None;
        currentDetailEditWidget = nullptr;
    }

    stagedTransactions.clear();
    stagedTransactionDeletions.clear();
    checkedTransactionIds.clear();

    // Reset the temp ID counter
    nextTempId = -1;

    loadTransactions();
    updateWidget();
}

void TransactionListWidget::updateButtonStates() {
    bool hasChanges = !(stagedTransactions.isEmpty() && stagedTransactionDeletions.isEmpty());

    // Disable save/discard when locked
    ui.saveButton->setVisible(hasChanges);
    ui.discardButton->setVisible(hasChanges);

    bool hasCheckedRows = !checkedTransactionIds.isEmpty();

    // Disable delete when locked
    ui.deleteButton->setVisible(hasCheckedRows);

    // Disable new transaction button when locked
    ui.newTransactionToolButton->setEnabled(!isTableLocked);

    // Disable import button when locked
    ui.importButton->setEnabled(!isTableLocked);
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

void TransactionListWidget::updateCountLabels() {
    int totalCount;
    int visibleCount;

    if (useLazyLoading && lazyLoader) {
        totalCount = lazyLoader->getTotalCount();
        visibleCount = lazyLoader->getLoadedCount();
    } else {
        totalCount = ui.transactionTableWidget->rowCount();
        visibleCount = totalCount;
    }

    int selectedCount = checkedTransactionIds.size();

    // Update visible count label
    if (totalCount > 0) {
        if (visibleCount < totalCount) {
            ui.visibleCountLabel->setText(QString("%1 of %2 visible").arg(visibleCount).arg(totalCount));
        } else {
            ui.visibleCountLabel->setText(QString("%1 visible").arg(visibleCount));
        }
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

void TransactionListWidget::insertTransactionRow(int row, const Transaction& transaction, bool isShadowed) {
    AccountDAO accountDao;
    CategoryDAO categoryDao;

    ui.transactionTableWidget->insertRow(row);

    // Set ID in vertical header
    QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString::number(transaction.getTransactionId()));
    ui.transactionTableWidget->setVerticalHeaderItem(row, idHeaderItem);

    // === Date ===
    QDateEdit* dateEdit = new QDateEdit(transaction.getDate());
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setCalendarPopup(true);
    dateEdit->setReadOnly(isTableLocked);
    dateEdit->setEnabled(!isTableLocked);
    if (! isTableLocked) {
        connect(dateEdit, &QDateEdit::dateChanged, this, [this, dateEdit]() {
            const int currentRow = rowForWidget(dateEdit);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
    }
    ui.transactionTableWidget->setCellWidget(row, 0, dateEdit);

    // === Account ===
    QComboBox* accountCombo = new QComboBox();

    if (isShadowed) {
        accountCombo->addItem("(Hidden)", SHADOWED_TRANSACTIONS_ID);
    }

    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
    for (const Account& acc : accounts) {
        accountCombo->addItem(acc. getName(), acc.getAccountId());
    }

    if (isShadowed) {
        accountCombo->setCurrentIndex(0);
    } else {
        int accountId = transaction.getAccountId();
        for (int i = 0; i < accountCombo->count(); ++i) {
            if (accountCombo->itemData(i).toInt() == accountId) {
                accountCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    accountCombo->setEnabled(!isTableLocked);
    if (!isTableLocked) {
        connect(accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, accountCombo]() {
            const int currentRow = rowForWidget(accountCombo);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
    }
    ui.transactionTableWidget->setCellWidget(row, 1, accountCombo);

    // === Payee ===
    QLineEdit* payeeEdit = new QLineEdit(transaction.getPayee());
    payeeEdit->setReadOnly(isTableLocked);
    if (!isTableLocked) {
        connect(payeeEdit, &QLineEdit::textChanged, this, [this, payeeEdit]() {
            const int currentRow = rowForWidget(payeeEdit);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
        setupDetailEditConnections(payeeEdit, transaction.getTransactionId(), DetailEditField::Payee);
    }
    ui.transactionTableWidget->setCellWidget(row, 2, payeeEdit);

    // === Description ===
    QLineEdit* descEdit = new QLineEdit(transaction. getDescription());
    descEdit->setReadOnly(isTableLocked);
    if (!isTableLocked) {
        connect(descEdit, &QLineEdit::textChanged, this, [this, descEdit]() {
            const int currentRow = rowForWidget(descEdit);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
        setupDetailEditConnections(descEdit, transaction.getTransactionId(), DetailEditField::Description);
    }
    ui.transactionTableWidget->setCellWidget(row, 3, descEdit);

    // === Category ===
    QComboBox* categoryCombo = new QComboBox();
    categoryCombo->addItem("(No Category)", 0);
    QList<Category> categories = categoryDao.getAllCategories();
    for (const Category& cat : categories) {
        categoryCombo->addItem(cat. getName(), cat.getCategoryId());
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

    categoryCombo->setEnabled(! isTableLocked);
    if (!isTableLocked) {
        connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, categoryCombo]() {
            const int currentRow = rowForWidget(categoryCombo);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
    }
    ui.transactionTableWidget->setCellWidget(row, 4, categoryCombo);

    // === Amount ===
    QDoubleSpinBox* amountSpin = new QDoubleSpinBox();
    amountSpin->setMaximum(1'000'000);
    amountSpin->setDecimals(2);
    amountSpin->setValue(transaction.getAmount());
    amountSpin->setReadOnly(isTableLocked);
    amountSpin->setEnabled(!isTableLocked);
    if (!isTableLocked) {
        connect(amountSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, amountSpin]() {
            const int currentRow = rowForWidget(amountSpin);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
        setupDetailEditConnections(amountSpin, transaction.getTransactionId(), DetailEditField::Amount);
    }
    ui.transactionTableWidget->setCellWidget(row, 5, amountSpin);

    // === Type ===
    QComboBox* typeCombo = new QComboBox();
    typeCombo->addItems({"expense", "income"});
    typeCombo->setCurrentText(transaction.getType());
    typeCombo->setEnabled(!isTableLocked);
    if (!isTableLocked) {
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, typeCombo]() {
            const int currentRow = rowForWidget(typeCombo);
            if (currentRow >= 0) stageTransactionChange(currentRow);
        });
    }
    ui.transactionTableWidget->setCellWidget(row, 6, typeCombo);

    // === Checkbox (always enabled) ===
    QCheckBox *checkBox = new QCheckBox();
    const int transactionId = transaction.getTransactionId();
    connect(checkBox, &QCheckBox::clicked, this, [this, checkBox, transactionId]() {
        handleTransactionCheckboxStateChange(transactionId, checkBox->isChecked());
    });
    QWidget *checkBoxWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
    layout->addWidget(checkBox);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    ui.transactionTableWidget->setCellWidget(row, 7, checkBoxWidget);
}

void TransactionListWidget::updatePinnedTransactions() {
    if (! useLazyLoading || !lazyLoader) return;

    QSet<int> pinnedIds;

    // Pin staged transactions
    for (auto it = stagedTransactions.begin(); it != stagedTransactions. end(); ++it) {
        pinnedIds.insert(it.key());
    }

    // Pin transactions marked for deletion
    pinnedIds.unite(stagedTransactionDeletions);

    // Pin checked rows
    pinnedIds.unite(checkedTransactionIds);

    lazyLoader->setPinnedTransactionIds(pinnedIds);
}

void TransactionListWidget::setupDetailEditConnections(QWidget* widget, int transactionId, DetailEditField field) {
    if (!useDetailEdit) {
        // If detail edit is disabled, make fields read-only
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            lineEdit->setReadOnly(false);  // Allow normal editing in table
        } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
            spinBox->setReadOnly(false);  // Allow normal editing in table
        }
        return;
    }

    // Detail edit is enabled - make fields read-only and set up event filter
    if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
        lineEdit->setReadOnly(true);  // Make read-only when detail edit is enabled
        lineEdit->installEventFilter(this);
        lineEdit->setProperty("detailEditTransactionId", transactionId);
        lineEdit->setProperty("detailEditField", static_cast<int>(field));

        // Change cursor to indicate it's clickable
        lineEdit->setCursor(Qt::PointingHandCursor);
    } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
        spinBox->setReadOnly(true);  // Make read-only when detail edit is enabled
        spinBox->installEventFilter(this);
        spinBox->setProperty("detailEditTransactionId", transactionId);
        spinBox->setProperty("detailEditField", static_cast<int>(field));

        // Change cursor to indicate it's clickable
        spinBox->setCursor(Qt::PointingHandCursor);
    }
}

bool TransactionListWidget::eventFilter(QObject* obj, QEvent* event) {
    if (!useDetailEdit) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {  // ADD MouseButtonPress
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && widget->property("detailEditTransactionId").isValid()) {
            int row = rowForTransactionId(widget->property("detailEditTransactionId").toInt());
            int fieldInt = widget->property("detailEditField").toInt();
            DetailEditField field = static_cast<DetailEditField>(fieldInt);

            openDetailEdit(row, field);
            return false;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void TransactionListWidget::syncDetailEditToTable() {
    if (! currentDetailEditWidget) {
        return;
    }

    //Safety check: verify the row still exists
    const int currentRow = rowForTransactionId(currentDetailTransactionId);
    if (currentRow < 0) {
        qDebug() << "syncDetailEditToTable: Transaction no longer exists, closing detail edit";
        currentDetailTransactionId = 0;
        currentDetailEditField = DetailEditField::None;
        currentDetailEditWidget = nullptr;
        ui.detailEditFrame->setVisible(false);
        return;
    }

    QString newText = ui.detailTextEdit->toPlainText();

    // Update the appropriate widget
    switch (currentDetailEditField) {
    case DetailEditField::Payee:
    case DetailEditField::Description:
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(currentDetailEditWidget)) {
            {
                const QSignalBlocker blocker(lineEdit);
                lineEdit->setText(newText);
            }
            stageTransactionChange(currentRow);
        }
        break;
    case DetailEditField::Amount:
        if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(currentDetailEditWidget)) {
            bool ok;
            double value = newText.toDouble(&ok);
            if (ok) {
                {
                    const QSignalBlocker blocker(spinBox);
                    spinBox->setValue(value);
                }
                stageTransactionChange(currentRow);
            }
        }
        break;

    default:
        break;
    }
}

void TransactionListWidget::updateEditableStates() {
    for (int row = 0; row < ui.transactionTableWidget->rowCount(); ++row) {
        // Date
        QWidget* dateWidget = ui.transactionTableWidget->cellWidget(row, 0);
        if (QDateEdit* dateEdit = qobject_cast<QDateEdit*>(dateWidget)) {
            dateEdit->setReadOnly(isTableLocked);
            dateEdit->setEnabled(!isTableLocked);
        }

        // Account
        QWidget* accountWidget = ui.transactionTableWidget->cellWidget(row, 1);
        if (QComboBox* accountCombo = qobject_cast<QComboBox*>(accountWidget)) {
            accountCombo->setEnabled(!isTableLocked);
        }

        // Payee
        QWidget* payeeWidget = ui.transactionTableWidget->cellWidget(row, 2);
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(payeeWidget)) {
            if (isTableLocked) {
                lineEdit->setReadOnly(true);
                lineEdit->setCursor(Qt::ArrowCursor);
                lineEdit->removeEventFilter(this);
            } else {
                setupDetailEditConnections(lineEdit, transactionIdForRow(row), DetailEditField::Payee);
            }
        }

        // Description
        QWidget* descWidget = ui.transactionTableWidget->cellWidget(row, 3);
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(descWidget)) {
            if (isTableLocked) {
                lineEdit->setReadOnly(true);
                lineEdit->setCursor(Qt::ArrowCursor);
                lineEdit->removeEventFilter(this);
            } else {
                setupDetailEditConnections(lineEdit, transactionIdForRow(row), DetailEditField::Description);
            }
        }

        // Category
        QWidget* categoryWidget = ui.transactionTableWidget->cellWidget(row, 4);
        if (QComboBox* categoryCombo = qobject_cast<QComboBox*>(categoryWidget)) {
            categoryCombo->setEnabled(!isTableLocked);
        }

        // Amount
        QWidget* amountWidget = ui.transactionTableWidget->cellWidget(row, 5);
        if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(amountWidget)) {
            if (isTableLocked) {
                spinBox->setReadOnly(true);
                spinBox->setEnabled(!isTableLocked);
                spinBox->setCursor(Qt:: ArrowCursor);
                spinBox->removeEventFilter(this);
            } else {
                setupDetailEditConnections(spinBox, transactionIdForRow(row), DetailEditField::Amount);
            }
        }

        // Type
        QWidget* typeWidget = ui. transactionTableWidget->cellWidget(row, 6);
        if (QComboBox* typeCombo = qobject_cast<QComboBox*>(typeWidget)) {
            typeCombo->setEnabled(!isTableLocked);
        }

        // Checkbox remains enabled when locked
    }
}

void TransactionListWidget::updateWidget() {
    // Update all UI elements in the correct order
    updateButtonStates();
    updateRowVisualState();
    updateCountLabels();
}

void TransactionListWidget::onImportTransactionsClicked()
{
    // Check if there are pending changes
    if (! stagedTransactions.isEmpty() || !stagedTransactionDeletions.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Pending Changes",
            QString("You have %1 pending change(s) that need to be saved or discarded.\n\n"
                    "Do you want to save them now before importing?")
                .arg(stagedTransactions.size() + stagedTransactionDeletions.size()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
            );

        if (reply == QMessageBox::Save) {
            // Save changes first
            saveChanges();

            // Check if save was successful (user might have cancelled due to validation errors)
            if (!stagedTransactions.isEmpty() || !stagedTransactionDeletions. isEmpty()) {
                // Still have pending changes - save must have failed
                QMessageBox::warning(this, "Cannot Import",
                                     "Please fix validation errors before importing.");
                return;
            }
        } else if (reply == QMessageBox::Discard) {
            // Discard changes
            discardChanges();
        } else {
            // Cancel - don't open import dialog
            return;
        }
    }

    // No pending changes - proceed with import
    ImportManagerDialog dialog(userId, this);

    connect(&dialog, &ImportManagerDialog::transactionsImported, [this](int count) {
        qDebug() << "Imported" << count << "transactions";

        if (count > 0) {
            // Refresh UI
            loadTransactions();
            updateWidget();
            emit transactionsChanged();
        }
    });

    dialog.exec();
}

void TransactionListWidget::onDetailEditTextChanged() {
    if (currentDetailTransactionId == 0 || !currentDetailEditWidget) {
        return;
    }

    // Safety check: verify the row still exists
    if (rowForTransactionId(currentDetailTransactionId) < 0) {
        qDebug() << "onDetailEditTextChanged: Transaction no longer exists, closing detail edit";
        closeDetailEdit();
        return;
    }

    syncDetailEditToTable();
}

void TransactionListWidget::openDetailEdit(int row, DetailEditField field) {
    if (!useDetailEdit) return;

    // Safety check: verify the row exists
    if (row < 0 || row >= ui. transactionTableWidget->rowCount()) {
        qDebug() << "openDetailEdit: Invalid row" << row;
        return;
    }

    // Get the widget for the field
    QWidget* widget = nullptr;
    QString fieldName;
    QString currentText;

    switch (field) {
    case DetailEditField:: Payee:
        widget = ui.transactionTableWidget->cellWidget(row, 2);
        fieldName = "Payer/Payee";
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            currentText = lineEdit->text();
        }
        break;

    case DetailEditField::Description:
        widget = ui.transactionTableWidget->cellWidget(row, 3);
        fieldName = "Description";
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            currentText = lineEdit->text();
        }
        break;

    case DetailEditField::Amount:
        widget = ui.transactionTableWidget->cellWidget(row, 5);
        fieldName = "Amount";
        if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
            currentText = QString::number(spinBox->value(), 'f', 2);
        }
        break;

    default:
        return;
    }

    if (! widget) {
        return;
    }

    // Store current editing context
    currentDetailTransactionId = transactionIdForRow(row);
    currentDetailEditField = field;
    currentDetailEditWidget = widget;

    // Update label
    QString transactionId = ui.transactionTableWidget->verticalHeaderItem(row)->text();
    ui.detailEditLabel->setText(QString("Editing: %1 (ID: %2)").arg(fieldName, transactionId));

    // Set text in detail edit area
    {
        const QSignalBlocker blocker(ui.detailTextEdit);
        ui.detailTextEdit->setPlainText(currentText);
    }

    // Show the frame
    ui.detailEditFrame->setVisible(true);

    // Focus the text edit
    ui.detailTextEdit->setFocus();
    ui.detailTextEdit->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    //ui.detailTextEdit->selectAll();
}

void TransactionListWidget::closeDetailEdit() {
    // Safety check: verify the row still exists before syncing
    if (currentDetailEditWidget && rowForTransactionId(currentDetailTransactionId) >= 0) {
        syncDetailEditToTable();
    } else if (currentDetailTransactionId != 0) {
        qDebug() << "closeDetailEdit: Transaction no longer exists, skipping sync";
    }

    ui.detailEditFrame->setVisible(false);
    currentDetailTransactionId = 0;
    currentDetailEditField = DetailEditField:: None;
    currentDetailEditWidget = nullptr;
}

void TransactionListWidget::onDetailEditCheckboxToggled(bool checked) {
    qDebug() << "Detail edit checkbox toggled to:" << checked;

    // Save the setting
    Settings::instance().setDetailEditEnabled(checked);

    // No need to update local state (useDetailEdit = checked), since that's done in TLW:reinitizalize

    // Close detail edit if it's being disabled
    if (!useDetailEdit && currentDetailTransactionId != 0) {
        closeDetailEdit();
    }

    // Update all existing widgets to match the new setting
    reinitialize();

    qDebug() << "Detail edit mode is now:" << (useDetailEdit ? "enabled" : "disabled");
}

void TransactionListWidget::onLockTableCheckboxToggled(bool checked) {
    qDebug() << "Lock table checkbox toggled to:" << checked;

    isTableLocked = checked;

    // Close detail edit if locking
    if (isTableLocked && currentDetailTransactionId != 0) {
        closeDetailEdit();
    }

    // Disable/enable detail edit checkbox based on lock state
    ui.detailEditCheckBox->setEnabled(!isTableLocked);

    // Update all widgets to reflect the lock state
    updateEditableStates();

    // Update button states
    updateButtonStates();

    qDebug() << "Table is now:" << (isTableLocked ? "locked" : "unlocked");
}
