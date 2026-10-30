#include "deleteaccountdialog.h"
#include "account.h"
#include "settings.h"
#include "theme.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {

QString counted(int count, const QString& singular) {
    return QString("%1 %2%3").arg(count).arg(singular, count == 1 ? "" : "s");
}

QLabel* addNote(const QString& objectName, QVBoxLayout* layout, QWidget* parent) {
    auto* note = new QLabel(parent);
    note->setObjectName(objectName);
    note->setTextFormat(Qt::PlainText);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(note);
    return note;
}

}

DeleteAccountDialog::DeleteAccountDialog(int userId, int accountId, QWidget* parent)
    : QDialog(parent), deletedAccountId(accountId) {
    setWindowTitle("Delete account");
    setMinimumWidth(420);

    AccountDAO dao;
    const Account account = dao.getAccountById(accountId);
    accountName = account.getName();
    balanceCents = account.getBalanceCents();
    transactions = dao.transactionCount(accountId);
    rules = dao.ruleCount(accountId);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto* title = new QLabel(QString("Delete \"%1\"?").arg(accountName), this);
    title->setTextFormat(Qt::PlainText);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    QStringList facts = {counted(transactions, "transaction"),
                         QString("%1 balance").arg(Settings::instance().formatCurrency(balanceCents))};
    if (rules > 0) {
        facts << QString("used by %1").arg(counted(rules, "rule"));
    }
    auto* summary = addNote("deleteSummaryLabel", layout, this);
    summary->setText(facts.join(", "));

    moveOption = new QRadioButton("Move transactions to another account", this);
    moveOption->setObjectName("moveTransactionsOption");
    layout->addWidget(moveOption);

    targetCombo = new QComboBox(this);
    targetCombo->setObjectName("targetAccountCombo");
    for (const Account& other : dao.getAllAccountsForUser(userId)) {
        if (other.getAccountId() != accountId) {
            targetCombo->addItem(other.getName(), other.getAccountId());
        }
    }
    layout->addWidget(targetCombo);
    moveNote = addNote("moveNoteLabel", layout, this);

    deleteOption = new QRadioButton("Delete the transactions too", this);
    deleteOption->setObjectName("deleteTransactionsOption");
    layout->addWidget(deleteOption);
    deleteNote = addNote("deleteNoteLabel", layout, this);

    // Moving keeps the data, so it comes first; without a place to move to only deleting is left
    const bool canMove = targetCombo->count() > 0;
    moveOption->setEnabled(canMove);
    targetCombo->setEnabled(canMove);
    (canMove ? moveOption : deleteOption)->setChecked(true);
    if (!canMove) {
        moveOption->setToolTip("You have no other account.");
    }

    // With no transactions there's nothing to choose
    if (transactions == 0) {
        moveOption->setVisible(false);
        targetCombo->setVisible(false);
        moveNote->setVisible(false);
        deleteOption->setVisible(false);
    }

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("deleteStatusLabel");
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText, Theme::colors().danger);
    statusLabel->setPalette(palette);
    layout->addWidget(statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    deleteButton = buttons->addButton("Delete", QDialogButtonBox::DestructiveRole);
    deleteButton->setObjectName("deleteAccountButton");
    deleteButton->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    connect(deleteButton, &QPushButton::clicked, this, &DeleteAccountDialog::deleteAccount);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(moveOption, &QRadioButton::toggled, this, &DeleteAccountDialog::updateTexts);
    connect(targetCombo, &QComboBox::currentIndexChanged, this, &DeleteAccountDialog::updateTexts);
    updateTexts();
}

void DeleteAccountDialog::updateTexts() {
    const bool moving = moveOption->isChecked();
    targetCombo->setEnabled(moving);

    if (targetCombo->count() > 0) {
        const Settings& settings = Settings::instance();
        const QString target = targetCombo->currentText();
        const Cents targetBalance = AccountDAO().getAccountById(targetCombo->currentData().toInt()).getBalanceCents();
        QString note = QString("%1's balance goes from %2 to %3.")
                           .arg(target, settings.formatCurrency(targetBalance),
                                settings.formatCurrency(targetBalance + balanceCents));
        if (rules > 0) {
            note += QString(" Rules for this account will apply to %1.").arg(target);
        }
        moveNote->setText(note);
    } else {
        moveNote->setText("You have no other account to move them to.");
    }

    QString note = transactions > 0 ? "This can't be undone. Reports and budgets lose this spending."
                                    : "This can't be undone.";
    if (rules > 0) {
        note += QString(" %1 will stop using this account, and any that need it are deleted.")
                    .arg(counted(rules, "rule"));
    }
    deleteNote->setText(note);

    if (transactions == 0) {
        deleteButton->setText(QString("Delete %1").arg(accountName));
    } else if (moving) {
        deleteButton->setText(QString("Move %1 and delete %2").arg(transactions).arg(accountName));
    } else {
        deleteButton->setText(QString("Delete %1 and %2").arg(accountName, counted(transactions, "transaction")));
    }
}

void DeleteAccountDialog::deleteAccount() {
    AccountDAO dao;
    const bool moving = transactions > 0 && moveOption->isChecked();
    const bool deleted = moving ? dao.deleteAccountMovingTo(deletedAccountId, targetCombo->currentData().toInt())
                                : dao.deleteAccount(deletedAccountId);
    if (!deleted) {
        statusLabel->setText("The account couldn't be deleted.");
        statusLabel->setVisible(true);
        return;
    }
    accept();
}
