#include "db/databasemanager.h"
#include "account.h"
#include "addaccountwidget.h"

#include <QSqlQuery>
#include <qsqlerror.h>

AddAccountWidget::AddAccountWidget(QWidget* parent)
    : QWidget(parent) {
    ui.setupUi(this);

    // The form is shown inside the "Add account" dialog, so the inline toggle buttons are not needed
    ui.addAccountButton->setVisible(false);
    ui.toolButton_2->setVisible(false);
    ui.line->setVisible(false);

    // Populate account types
    AccountDAO accountDao;
    QList<QString> accountTypes = accountDao.getAccountTypes();
    for (const QString& type : accountTypes) {
        ui.accountTypeComboBox->addItem(type);
    }

    connect(ui.createAccountButton, &QAbstractButton::clicked, this, &AddAccountWidget::onCreateAccountClicked);
}

void AddAccountWidget::onCreateAccountClicked() {
    // Validate userId
    if (userId == -1) {
        QMessageBox::warning(this, "Error", "User ID is not set. Cannot create account.");
        return;
    }

    // Get input values
    QString accountName = ui.accountNameInput->text().trimmed();
    QString accountType = ui.accountTypeComboBox->currentText();
    double initialBalance = ui.initialBalanceInput->value();

    // Validate inputs
    if (accountName.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid account name.");
        return;
    }
    if (accountType.isEmpty()) {
        QMessageBox::warning(this, "Invalid Input", "Please select an account type.");
        return;
    }

    // Use the DAO to create the account
    int accountId = AccountDAO::createAccount(userId, accountName, accountType, initialBalance);

    if (accountId == -1) {
        QMessageBox::critical(this, "Error", "Failed to create account. Please try again.");
        return;
    }

    // Clear input fields
    ui.accountNameInput->clear();
    ui.accountTypeComboBox->setCurrentIndex(0);
    ui.initialBalanceInput->setValue(0);

    // Emit a signal to notify other components
    emit accountCreated();
}

void AddAccountWidget::setUserId(int newUserId) {
    userId = newUserId;
}
