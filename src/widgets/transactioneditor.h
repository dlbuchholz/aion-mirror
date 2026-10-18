#ifndef TRANSACTIONEDITOR_H
#define TRANSACTIONEDITOR_H

#include <QFrame>
#include "category.h"
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

    int transactionId = -1;
    QList<Category> categories;

    QLabel* titleLabel = nullptr;
    QButtonGroup* typeGroup = nullptr;
    QPushButton* expenseButton = nullptr;
    QPushButton* incomeButton = nullptr;
    QDoubleSpinBox* amountEdit = nullptr;
    QLineEdit* payeeEdit = nullptr;
    QCompleter* payeeCompleter = nullptr;
    QDateEdit* dateEdit = nullptr;
    QComboBox* categoryCombo = nullptr;
    QComboBox* accountCombo = nullptr;
    QTextEdit* descriptionEdit = nullptr;
    QLabel* errorLabel = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // TRANSACTIONEDITOR_H
