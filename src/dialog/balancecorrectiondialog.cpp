#include "balancecorrectiondialog.h"
#include "account.h"
#include "balance.h"
#include "csvimport.h"
#include "settings.h"
#include "theme.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

BalanceCorrectionDialog::BalanceCorrectionDialog(int userId, int accountId, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Correct balance");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    auto* intro = new QLabel("If Aion shows a different balance than your bank, add a correction for the difference. "
                             "It counts towards the balance, not towards income or spending.", this);
    intro->setWordWrap(true);
    intro->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(intro);

    auto* form = new QFormLayout();
    form->setVerticalSpacing(8);
    accountCombo = new QComboBox(this);
    accountCombo->setObjectName("correctionAccountCombo");
    for (const Account& account : AccountDAO().getAllAccountsForUser(userId)) {
        accountCombo->addItem(account.getName(), account.getAccountId());
    }
    accountCombo->setCurrentIndex(qMax(0, accountCombo->findData(accountId)));
    form->addRow("Account", accountCombo);

    dateEdit = new QDateEdit(QDate::currentDate(), this);
    dateEdit->setObjectName("correctionDateEdit");
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd.MM.yyyy");
    form->addRow("On", dateEdit);

    currentLabel = new QLabel(this);
    currentLabel->setObjectName("currentBalanceLabel");
    form->addRow("Balance in Aion", currentLabel);

    expectedEdit = new QLineEdit(this);
    expectedEdit->setObjectName("bankBalanceEdit");
    expectedEdit->setPlaceholderText("As shown by your bank");
    expectedEdit->setAlignment(Qt::AlignRight);
    form->addRow("Your bank's balance", expectedEdit);
    layout->addLayout(form);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("correctionStatusLabel");
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    addButton = buttons->addButton("Add correction", QDialogButtonBox::AcceptRole);
    addButton->setObjectName("addCorrectionButton");
    addButton->setDefault(true);
    connect(addButton, &QPushButton::clicked, this, &BalanceCorrectionDialog::addCorrection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(accountCombo, &QComboBox::currentIndexChanged, this, &BalanceCorrectionDialog::updateBalance);
    connect(dateEdit, &QDateEdit::dateChanged, this, &BalanceCorrectionDialog::updateBalance);
    connect(expectedEdit, &QLineEdit::textChanged, this, &BalanceCorrectionDialog::updateBalance);
    updateBalance();
    expectedEdit->setFocus();
}

int BalanceCorrectionDialog::addedTransactionId() const {
    return transactionId;
}

bool BalanceCorrectionDialog::expectedBalance(Cents* cents) const {
    if (expectedEdit->text().trimmed().isEmpty()) {
        return false;
    }
    const QChar separator = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
    bool ok = false;
    *cents = CsvImport::parseAmount(expectedEdit->text(), separator, &ok);
    return ok;
}

void BalanceCorrectionDialog::updateBalance() {
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    const int accountId = accountCombo->currentData().toInt();
    currentCents = accountCombo->count() > 0 ? Balance::onDate(accountId, dateEdit->date()) : 0;
    currentLabel->setText(settings.formatCurrency(currentCents));

    Cents expected = 0;
    QColor color = colors.textSecondary;
    bool canAdd = false;
    if (accountCombo->count() == 0) {
        statusLabel->setText("Add an account first.");
    } else if (expectedEdit->text().trimmed().isEmpty()) {
        statusLabel->setText("Enter the balance your bank shows for the end of that day.");
    } else if (!expectedBalance(&expected)) {
        statusLabel->setText("That isn't an amount Aion can read.");
        color = colors.danger;
    } else if (expected == currentCents) {
        statusLabel->setText("Aion already shows this balance. Nothing to correct.");
        color = colors.positive;
    } else {
        statusLabel->setText(QString("Adds a balance correction of %1 on %2.")
                                 .arg(settings.formatCurrency(expected - currentCents, true),
                                      QLocale(QLocale::English).toString(dateEdit->date(), "d MMMM yyyy")));
        color = colors.textPrimary;
        canAdd = true;
    }
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText, color);
    statusLabel->setPalette(palette);
    addButton->setEnabled(canAdd);
}

void BalanceCorrectionDialog::addCorrection() {
    Cents expected = 0;
    if (!expectedBalance(&expected) || expected == currentCents) {
        return;
    }
    transactionId = Balance::addCorrection(accountCombo->currentData().toInt(), dateEdit->date(), expected - currentCents,
                                           "Corrected by hand");
    if (transactionId <= 0) {
        statusLabel->setText("The correction couldn't be saved.");
        return;
    }
    accept();
}
