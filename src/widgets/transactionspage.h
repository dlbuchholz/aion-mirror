#ifndef TRANSACTIONSPAGE_H
#define TRANSACTIONSPAGE_H

#include <QWidget>
#include <functional>
#include "category.h"
#include "rule.h"
#include "transaction.h"

class QAction;
class QButtonGroup;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;
class QStackedWidget;
class QToolButton;
class TransactionDelegate;
class TransactionEditor;
class TransactionListModel;

// Ledger view: header for the shown account, filters, transactions grouped by day,
// an editor panel that saves immediately, bulk actions and totals.
class TransactionsPage : public QWidget {
    Q_OBJECT

public:
    static constexpr int ALL_ACCOUNTS = -1;

    explicit TransactionsPage(QWidget* parent = nullptr);

    // Asks before deleting (a message box; tests replace it)
    std::function<bool(const QString& title, const QString& text)> confirm;

    void setUserId(int userId);
    void showAccount(int accountId);                 // ALL_ACCOUNTS for every account of the user
    void reload();                                   // Re-read accounts, categories and transactions
    void applySettings();                            // Number formats changed
    void showCategoryThisMonth(int categoryId);      // One category's transactions this month (0 = no category)
    void openTransaction(int transactionId);         // Shows all transactions and opens this one in the editor

public slots:
    void addTransaction();
    void importTransactions();
    void correctBalance(int accountId);              // ALL_ACCOUNTS: the dialog lets the user pick one

signals:
    void transactionsChanged();
    void createRuleRequested(const Rule& draft);     // "Create rule from this…" on a transaction

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onSelectionChanged();
    void openEditor(const QModelIndex& index);
    void showContextMenu(const QPoint& position);   // Right-click on a transaction
    void selectDay(const QModelIndex& header, bool extendRange);  // Ctrl/Shift-click on a day header
    void onSaveRequested(const Transaction& transaction);
    void deleteSelected();
    void deleteTransactions(const QList<int>& transactionIds);
    void closeEditor();

private:
    enum class Period {
        AllTime,
        ThisMonth,
        LastMonth,
        ThisYear
    };

    QToolButton* addChip(const QString& text, QLayout* layout);
    void applyFilter();
    void populateCategoryFilter();
    void updateHeader();
    void updateTotals();
    void updateBulkBar();
    QList<int> selectedTransactionIds() const;
    void restoreSelection(const QList<int>& transactionIds);   // After the model was rebuilt
    void setCategoryForSelection(int categoryId);
    int defaultAccountForNew() const;
    void updateIcons();                              // Re-tint icons after a theme change

    int userId = -1;
    int accountId = ALL_ACCOUNTS;
    bool showingUnassigned = false;
    QList<Category> categories;
    QList<QPair<int, QString>> accounts;
    int unassignedCount = 0;

    TransactionListModel* model = nullptr;
    TransactionDelegate* delegate = nullptr;

    QLabel* titleLabel = nullptr;
    QLabel* subtitleLabel = nullptr;
    QPushButton* importButton = nullptr;
    QPushButton* correctBalanceButton = nullptr;
    QLineEdit* searchEdit = nullptr;
    QAction* searchAction = nullptr;
    QButtonGroup* periodGroup = nullptr;
    QComboBox* categoryFilter = nullptr;
    QToolButton* unassignedChip = nullptr;
    QFrame* bulkBar = nullptr;
    QLabel* bulkLabel = nullptr;
    QToolButton* bulkCategoryButton = nullptr;
    QStackedWidget* listStack = nullptr;
    QListView* listView = nullptr;
    QLabel* emptyLabel = nullptr;
    TransactionEditor* editor = nullptr;
    QLabel* incomeLabel = nullptr;
    QLabel* expensesLabel = nullptr;
    QLabel* netLabel = nullptr;
};

#endif // TRANSACTIONSPAGE_H
