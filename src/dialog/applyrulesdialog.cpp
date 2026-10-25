#include "applyrulesdialog.h"
#include "account.h"
#include "settings.h"
#include "widgets/categorytag.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int MaxRowsShown = 200;                    // The rest is summed up below the list

QWidget* categoryCell(const Category* category, QWidget* parent) {
    if (!category) {
        auto* none = new QLabel("none", parent);
        none->setForegroundRole(QPalette::PlaceholderText);
        return none;
    }
    auto* tag = new CategoryTag(parent);
    tag->setCategory(category->getName(), category->getColor());
    return tag;
}

} // namespace

ApplyRulesDialog::ApplyRulesDialog(int userId, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Apply rules to existing transactions");
    resize(640, 480);

    rules = RuleDAO(userId).rules();
    for (const Category& category : CategoryDAO(userId).getAllCategories()) {
        categoryById.insert(category.getCategoryId(), category);
        if (category.isBuiltIn()) protectedCategories.append(category.getCategoryId());
    }
    TransactionDAO transactionDao;
    for (const Account& account : AccountDAO().getAllAccountsForUser(userId)) {
        transactions += transactionDao.getTransactionsForAccount(account.getAccountId());
    }
    std::sort(transactions.begin(), transactions.end(), [](const Transaction& a, const Transaction& b) {
        return a.getDate() > b.getDate();               // Newest first, like the transaction list
    });

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel("Your rules would change these transactions: their category, their payee's name, or "
                             "both. Nothing changes until you confirm.", this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    onlyUncategorisedCheck = new QCheckBox("Only transactions without a category", this);
    onlyUncategorisedCheck->setObjectName("onlyUncategorisedCheck");
    onlyUncategorisedCheck->setChecked(true);
    onlyUncategorisedCheck->setToolTip("Untick to also change the category of transactions that have one. Payees "
                                       "are renamed either way. Starting balances and balance corrections are never changed.");
    connect(onlyUncategorisedCheck, &QCheckBox::toggled, this, &ApplyRulesDialog::updatePreview);
    root->addWidget(onlyUncategorisedCheck);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    listLayout = new QVBoxLayout(content);
    listLayout->setContentsMargins(8, 8, 8, 8);
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    summaryLabel = new QLabel(this);
    summaryLabel->setObjectName("applyRulesSummary");
    summaryLabel->setForegroundRole(QPalette::PlaceholderText);
    root->addWidget(summaryLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    applyButton = buttons->addButton("Change", QDialogButtonBox::AcceptRole);
    applyButton->setObjectName("applyRulesButton");
    applyButton->setDefault(true);
    connect(applyButton, &QPushButton::clicked, this, &ApplyRulesDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    updatePreview();
}

void ApplyRulesDialog::updatePreview() {
    pending = Rules::changesFor(rules, transactions, onlyUncategorisedCheck->isChecked(), protectedCategories);

    while (QLayoutItem* item = listLayout->takeAt(0)) {
        if (item->layout()) {
            while (QLayoutItem* child = item->layout()->takeAt(0)) {
                delete child->widget();
                delete child;
            }
        }
        delete item->widget();
        delete item;
    }

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    QWidget* parent = listLayout->parentWidget();
    const Settings& settings = Settings::instance();
    for (int row = 0; row < qMin<qsizetype>(pending.size(), MaxRowsShown); ++row) {
        const Transaction& transaction = pending[row].transaction;
        const Cents signedAmount = transaction.getType() == "income" ? transaction.getAmountCents() : -transaction.getAmountCents();
        const RuleChange& change = pending[row];
        QString payee = transaction.getPayee().isEmpty() ? transaction.getDescription() : transaction.getPayee();
        if (!change.payee.isEmpty()) {
            payee = QString("%1 → %2").arg(transaction.getPayee(), change.payee);   // Renamed
        }
        grid->addWidget(new QLabel(QLocale().toString(transaction.getDate(), QLocale::ShortFormat), parent), row, 0);
        auto* payeeLabel = new QLabel(payee, parent);
        payeeLabel->setObjectName("applyRulesPayee");
        grid->addWidget(payeeLabel, row, 1);
        auto* amount = new QLabel(settings.formatCurrency(signedAmount), parent);
        amount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(amount, row, 2);
        // The category, and what it becomes if the rules change it
        const auto current = categoryById.constFind(transaction.getCategoryId());
        grid->addWidget(categoryCell(current != categoryById.constEnd() ? &*current : nullptr, parent), row, 3);
        if (change.categoryId > 0) {
            grid->addWidget(new QLabel("→", parent), row, 4);
            const auto target = categoryById.constFind(change.categoryId);
            grid->addWidget(categoryCell(target != categoryById.constEnd() ? &*target : nullptr, parent), row, 5);
        }
    }
    grid->setColumnStretch(1, 1);
    listLayout->addLayout(grid);
    listLayout->addStretch();

    const qsizetype count = pending.size();
    if (count == 0) {
        summaryLabel->setText(rules.isEmpty() ? "You have no rules yet." : "Your rules wouldn't change any transaction.");
    } else if (count > MaxRowsShown) {
        summaryLabel->setText(QString("Showing %1 of %2 transactions.").arg(MaxRowsShown).arg(count));
    } else {
        summaryLabel->setText(QString());
    }
    applyButton->setText(count == 1 ? QString("Change 1 transaction") : QString("Change %1 transactions").arg(count));
    applyButton->setVisible(count > 0);
}

void ApplyRulesDialog::apply() {
    if (!Rules::apply(pending)) {
        QMessageBox::warning(this, "Apply rules", "The transactions couldn't be changed. Nothing was changed.");
        return;
    }
    accept();
}
