#ifndef EDITACCOUNTDIALOG_H
#define EDITACCOUNTDIALOG_H

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Changes an account's name and type
class EditAccountDialog : public QDialog {
    Q_OBJECT

public:
    EditAccountDialog(int userId, int accountId, QWidget* parent = nullptr);

private:
    void updateStatus();
    void save();

    int ownerId;
    int editedAccountId;

    QLineEdit* nameEdit = nullptr;
    QComboBox* typeCombo = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* saveButton = nullptr;
};

#endif // EDITACCOUNTDIALOG_H
