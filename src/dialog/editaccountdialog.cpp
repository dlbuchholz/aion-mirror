#include "editaccountdialog.h"
#include "account.h"
#include "theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

EditAccountDialog::EditAccountDialog(int userId, int accountId, QWidget* parent)
    : QDialog(parent), ownerId(userId), editedAccountId(accountId) {
    setWindowTitle(tr("Edit account"));
    setMinimumWidth(360);

    AccountDAO dao;
    const Account account = dao.getAccountById(accountId);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* form = new QFormLayout();
    form->setVerticalSpacing(8);
    nameEdit = new QLineEdit(account.getName(), this);
    nameEdit->setObjectName("accountNameEdit");
    form->addRow(tr("Name"), nameEdit);

    typeCombo = new QComboBox(this);
    typeCombo->setObjectName("accountTypeCombo");
    for (const auto& type : dao.getAllAccountTypes(userId)) {
        typeCombo->addItem(type.second, type.first.toInt());
    }
    typeCombo->setCurrentIndex(qMax(0, typeCombo->findText(account.getType())));
    form->addRow(tr("Type"), typeCombo);
    layout->addLayout(form);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("accountStatusLabel");
    statusLabel->setWordWrap(true);
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText, Theme::colors().danger);
    statusLabel->setPalette(palette);
    layout->addWidget(statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    saveButton = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    saveButton->setObjectName("saveAccountButton");
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &EditAccountDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(nameEdit, &QLineEdit::textChanged, this, &EditAccountDialog::updateStatus);
    updateStatus();
    nameEdit->selectAll();
    nameEdit->setFocus();
}

void EditAccountDialog::updateStatus() {
    const QString name = nameEdit->text().trimmed();
    const bool taken = AccountDAO().isNameTaken(ownerId, name, editedAccountId);
    statusLabel->setText(taken ? tr("You already have an account with this name.") : QString());
    statusLabel->setVisible(taken);
    saveButton->setEnabled(!name.isEmpty() && !taken);
}

void EditAccountDialog::save() {
    if (!AccountDAO().editAccount(editedAccountId, nameEdit->text(), typeCombo->currentData().toInt())) {
        statusLabel->setText(tr("The account couldn't be saved."));
        statusLabel->setVisible(true);
        return;
    }
    accept();
}
