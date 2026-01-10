#ifndef TRANSACTIONLISTWIDGET_H
#define TRANSACTIONLISTWIDGET_H

#include <QWidget>
#include <QSet>
#include <QMap>
#include "lazytableloader.h"
#include "transaction.h"
#include "../../ui/ui_transactionlistwidget.h"

class TransactionListWidget : public QWidget {
    Q_OBJECT

public:
    explicit TransactionListWidget(QWidget* parent = nullptr);
    void reinitialize();
    void setUserId(int userId);
    void loadTransactions();                        // Load transactions for selected accounts
    void updateWidget();                             // General function that calls all other update UI functions

signals:
    void transactionsChanged();                     // Emit a signal when transactions were changed permanently

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void addEmptyTransactionRow();
    void deleteTransaction();                       // Delete selected transactions
    void saveChanges();                             // Save staged changes
    void discardChanges();                          // Discard staged changes
    void onImportTransactionsClicked();             // Open ImportDialog
    void onDetailEditTextChanged();                 // Handle text changes in detail edit area
    void closeDetailEdit();                         // Close the detail edit area
    void onDetailEditCheckboxToggled(bool checked); // Handle detail edit checkbox toggle
    void onLockTableCheckboxToggled(bool checked);  // Handle lock table checkbox toggle

private:
    static constexpr int SHADOWED_TRANSACTIONS_ID = -999; // Special constant for shadowed (hidden) transactions

    enum DetailEditField {
        None,
        Payee,
        Description,
        Amount
    };

    void setAccountIds(const QSet<int>& accountIds); // Set selected account IDs
    void populateComboBox();                         // Populate the combo box with accounts
    void stageTransactionChange(int row);            // Stage changes for a transaction
    void handleTransactionCheckboxStateChange(int row, bool checked);  // Track transaction deletions
    void updateButtonStates();                       // Show/hide Save and Discard buttons
    void updateRowVisualState();                     // Update visual state of rows based on staged changes
    void updateCountLabels();                        // Update "visible" and "selected" labels for transactions

    void insertTransactionRow(int row, const Transaction& transaction, bool isShadowed);
    void updatePinnedTransactions();

    void setupDetailEditConnections(QWidget* widget, int row, DetailEditField field);
    void openDetailEdit(int row, DetailEditField field);
    void syncDetailEditToTable();

    void updateEditableStates();                     // Update editable/read-only states based on lock

    Ui::TransactionListWidget ui;
    int userId;
    QSet<int> selectedAccountIds;                    // Set of selected account IDs
    QSet<int> checkedRows;

    // Staging containers
    QMap<int, Transaction> stagedTransactions;       // Staged transaction changes (ID -> Transaction)
    QSet<int> stagedTransactionDeletions;            // Staged transaction IDs for deletion

    int nextTempId; // Counter for temporary (staged) IDs

    LazyTableLoader* lazyLoader;
    bool useLazyLoading;
    bool useDetailEdit;
    bool isTableLocked;

    int currentDetailEditRow;
    DetailEditField currentDetailEditField;
    QWidget* currentDetailEditWidget;
};

#endif // TRANSACTIONLISTWIDGET_H
