#ifndef DELETEACCOUNTDIALOG_H
#define DELETEACCOUNTDIALOG_H

#include <QDialog>
#include "money.h"

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;

// Deletes an account: its transactions go to another account, or are deleted with it
class DeleteAccountDialog : public QDialog {
    Q_OBJECT

public:
    DeleteAccountDialog(int userId, int accountId, QWidget* parent = nullptr);

private:
    void updateTexts();
    void deleteAccount();

    int deletedAccountId;
    QString accountName;
    Cents balanceCents = 0;
    int transactions = 0;
    int rules = 0;

    QRadioButton* moveOption = nullptr;
    QRadioButton* deleteOption = nullptr;
    QComboBox* targetCombo = nullptr;
    QLabel* moveNote = nullptr;
    QLabel* deleteNote = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // DELETEACCOUNTDIALOG_H
