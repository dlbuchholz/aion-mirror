#ifndef TRANSACTIONLISTWIDGET_H
#define TRANSACTIONLISTWIDGET_H

#include <QWidget>
#include <QSet>
#include <QMap>
#include "transaction.h"
#include "../../ui/ui_transactionlistwidget.h"

class TransactionListWidget : public QWidget {
    Q_OBJECT

public:
    explicit TransactionListWidget(QWidget* parent = nullptr);

    void setUserId(int userId);
    void loadTransactions();                        // Load transactions for selected accounts

private slots:
    void addEmptyTransactionRow();
    void deleteTransaction();                       // Delete selected transactions
    void saveChanges();                             // Save staged changes
    void discardChanges();                          // Discard staged changes

private:
    static constexpr int SHADOWED_TRANSACTIONS_ID = -999; // Special constant for shadowed (hidden) transactions

    void setAccountIds(const QSet<int>& accountIds); // Set selected account IDs
    void populateComboBox();                         // Populate the combo box with accounts
    void stageTransactionChange(int row);            // Stage changes for a transaction
    void handleTransactionCheckboxStateChange(int row, bool checked);  // Track transaction deletions
    void updateButtonStates();                       // Show/hide Save and Discard buttons
    void updateRowVisualState();                     // Update visual state of rows based on staged changes
    void updateCountLabels();                        // Update "visible" and "selected" labels for transactions
    void updateWidget();                             // General function that calls all other update UI functions

    Ui::TransactionListWidget ui;
    int userId;
    QSet<int> selectedAccountIds;                    // Set of selected account IDs
    QSet<int> checkedRows;

    // Staging containers
    QMap<int, Transaction> stagedTransactions;       // Staged transaction changes (ID -> Transaction)
    QSet<int> stagedTransactionDeletions;            // Staged transaction IDs for deletion

    int nextTempId; // Counter for temporary (staged) IDs
};

#endif // TRANSACTIONLISTWIDGET_H
