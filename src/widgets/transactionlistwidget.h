#ifndef TRANSACTIONLISTWIDGET_H
#define TRANSACTIONLISTWIDGET_H

#include <QWidget>
#include <QSet>
#include "../../ui/ui_transactionlistwidget.h"

class TransactionListWidget : public QWidget {
    Q_OBJECT

public:
    explicit TransactionListWidget(QWidget* parent = nullptr);

    void setUserId(int userId);
    void loadTransactions();                        // Load transactions for selected accounts

private slots:
    void addEmptyTransactionRow();
private:
    void setAccountIds(const QSet<int>& accountIds); // Set selected account IDs
    void populateComboBox();         // Populate the combo box with accounts

    Ui::TransactionListWidget ui;
    int userId;
    QSet<int> selectedAccountIds; // Set of selected account IDs
};

#endif // TRANSACTIONLISTWIDGET_H
