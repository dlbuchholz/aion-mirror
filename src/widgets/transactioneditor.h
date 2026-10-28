#ifndef TRANSACTIONEDITOR_H
#define TRANSACTIONEDITOR_H

#include <QFrame>
#include "category.h"
#include "rule.h"
#include "transaction.h"

class QButtonGroup;
class QComboBox;
class QCompleter;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

// Side panel for creating or editing a single transaction. It does not touch the
// database itself: the transactions page persists what saveRequested() carries.
class TransactionEditor : public QFrame {
    Q_OBJECT

public:
    explicit TransactionEditor(QWidget* parent = nullptr);

    void setAccounts(const QList<QPair<int, QString>>& accounts);
    void setCategories(const QList<Category>& categories);
    void setPayeeSuggestions(const QStringList& payees);
    // New transactions get the category of the first matching rule while they're filled in,
    // until a category is chosen by hand
    void setRules(const QList<Rule>& rules);
    void applySettings();                            // Decimal separator for the amount field

    void newTransaction(const QDate& date, int accountId);
    void editTransaction(const Transaction& transaction);
    bool isEditingNew() const;
    int editedTransactionId() const;                 // -1 while creating a new transaction

    void showError(const QString& message);

signals:
    void saveRequested(const Transaction& transaction);
    void deleteRequested(int transactionId);
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString currentType() const;
    void setType(const QString& type);
    void populateCategories(int selectedCategoryId);
    void selectAccount(int accountId);
    void requestSave();
    void suggestFromRules();                         // Category and new payee name, for new transactions
    void acceptRename();
    void showOriginalPayee();
    void restoreOriginalPayee();
    void showRuleIcon(QLabel* icon);                 // The sparkle before a rule's suggestion
    QHash<int, QString> accountNames() const;        // For describing rules

    int transactionId = -1;
    QList<Category> categories;
    QList<Rule> rules;
    bool categoryChosenByHand = false;               // Then rules stop suggesting
    QString originalPayee;                           // Before a rename; saved with the transaction
    QString suggestedPayee;                          // What the rename note offers
    int suggestedRuleId = -1;                        // ... and from which rule
    int renamedByRuleId = -1;                        // The rule that gave the payee its name
    QString ruleGivenPayee;                          // That name; typing another one drops the rule

    QLabel* titleLabel = nullptr;
    QButtonGroup* typeGroup = nullptr;
    QPushButton* expenseButton = nullptr;
    QPushButton* incomeButton = nullptr;
    QDoubleSpinBox* amountEdit = nullptr;
    QLineEdit* payeeEdit = nullptr;
    QCompleter* payeeCompleter = nullptr;
    QDateEdit* dateEdit = nullptr;
    QComboBox* categoryCombo = nullptr;
    QWidget* suggestionNote = nullptr;               // Which rule suggested the category, and Undo
    QWidget* renameNote = nullptr;                   // A rule's name for the payee, and Rename
    QLabel* renameIcon = nullptr;
    QLabel* renameLabel = nullptr;
    QLabel* originalPayeeLabel = nullptr;            // "Originally: ... · Restore"
    QLabel* suggestionIcon = nullptr;
    QLabel* suggestionLabel = nullptr;
    QComboBox* accountCombo = nullptr;
    QTextEdit* descriptionEdit = nullptr;
    QLabel* errorLabel = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // TRANSACTIONEDITOR_H
