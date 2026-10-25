#include "renamedpayeesdialog.h"
#include "db/databasemanager.h"
#include "settings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

RenamedPayeesDialog::RenamedPayeesDialog(const QList<Transaction>& renamed, const QString& oldPayee,
                                         const QString& rename, int rule, QWidget* parent)
    : QDialog(parent), transactions(renamed), newPayee(rename), ruleId(rule) {
    setWindowTitle(restores() ? "Restore original names?" : QString("Rename to \"%1\"?").arg(newPayee));
    resize(600, 420);

    auto* root = new QVBoxLayout(this);
    QString renamedWhat = transactions.size() == 1 ? QString("1 transaction")
                                                   : QString("%1 transactions").arg(transactions.size());
    if (!oldPayee.isEmpty()) {
        renamedWhat += QString(" to \"%1\"").arg(oldPayee);
    }
    auto* intro = new QLabel(restores()
        ? QString("This rule renamed %1. Restore their original names, or keep the names they have now?").arg(renamedWhat)
        : QString("This rule renamed %1. Give them the new name \"%2\" too?").arg(renamedWhat, newPayee),
        this);
    intro->setObjectName("renamedPayeesIntro");
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    const Settings& settings = Settings::instance();
    for (int row = 0; row < transactions.size(); ++row) {
        const Transaction& transaction = transactions[row];
        const QString target = restores() ? transaction.getOriginalPayee() : newPayee;
        auto* check = new QCheckBox(content);
        check->setObjectName("renamedPayeeCheck");
        check->setChecked(true);
        connect(check, &QCheckBox::toggled, this, &RenamedPayeesDialog::updateButton);
        checks.append(check);
        grid->addWidget(check, row, 0);
        grid->addWidget(new QLabel(QLocale().toString(transaction.getDate(), QLocale::ShortFormat), content), row, 1);
        grid->addWidget(new QLabel(QString("%1 → %2").arg(transaction.getPayee(), target), content), row, 2);
        const Cents signedAmount = transaction.getType() == "income" ? transaction.getAmountCents()
                                                                     : -transaction.getAmountCents();
        auto* amount = new QLabel(settings.formatCurrency(signedAmount), content);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(amount, row, 3);
    }
    grid->setColumnStretch(2, 1);
    contentLayout->addLayout(grid);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* keepButton = buttons->addButton("Keep names", QDialogButtonBox::RejectRole);
    keepButton->setObjectName("keepNamesButton");
    applyButton = buttons->addButton(QString(), QDialogButtonBox::AcceptRole);
    applyButton->setObjectName("renamedPayeesApplyButton");
    applyButton->setDefault(true);
    connect(applyButton, &QPushButton::clicked, this, &RenamedPayeesDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    updateButton();
}

void RenamedPayeesDialog::updateButton() {
    qsizetype ticked = 0;
    for (const QCheckBox* check : std::as_const(checks)) {
        if (check->isChecked()) ++ticked;
    }
    applyButton->setText(QString("%1 %2").arg(restores() ? "Restore" : "Rename").arg(ticked));
    applyButton->setEnabled(ticked > 0);
}

void RenamedPayeesDialog::apply() {
    // All or none
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        QMessageBox::warning(this, windowTitle(), "The names couldn't be changed.");
        return;
    }
    TransactionDAO dao;
    for (int i = 0; i < transactions.size(); ++i) {
        if (!checks[i]->isChecked()) continue;
        Transaction changed = transactions[i];
        if (restores()) {
            changed.restoreOriginalPayee();
        } else {
            changed.renamePayee(newPayee, ruleId);
        }
        if (!dao.updateTransaction(changed)) {
            db.rollback();
            QMessageBox::warning(this, windowTitle(), "The names couldn't be changed. Nothing was changed.");
            return;
        }
    }
    if (!db.commit()) {
        QMessageBox::warning(this, windowTitle(), "The names couldn't be changed. Nothing was changed.");
        return;
    }
    accept();
}
