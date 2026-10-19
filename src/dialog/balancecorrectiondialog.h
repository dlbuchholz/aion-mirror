#ifndef BALANCECORRECTIONDIALOG_H
#define BALANCECORRECTIONDIALOG_H

#include <QDialog>
#include "money.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;

// Sets an account's balance on a day to what the bank shows by adding a "Balance correction"
// transaction for the difference
class BalanceCorrectionDialog : public QDialog {
    Q_OBJECT

public:
    BalanceCorrectionDialog(int userId, int accountId, QWidget* parent = nullptr);

    int addedTransactionId() const;                  // After accept()

private:
    void updateBalance();
    bool expectedBalance(Cents* cents) const;
    void addCorrection();

    Cents currentCents = 0;
    int transactionId = -1;

    QComboBox* accountCombo = nullptr;
    QDateEdit* dateEdit = nullptr;
    QLabel* currentLabel = nullptr;
    QLineEdit* expectedEdit = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* addButton = nullptr;
};

#endif // BALANCECORRECTIONDIALOG_H
