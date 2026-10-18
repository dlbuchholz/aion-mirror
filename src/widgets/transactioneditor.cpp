#include "transactioneditor.h"
#include "settings.h"
#include "theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int NO_ACCOUNT = -1;
constexpr int NO_CATEGORY = 0;            // Stored as NULL by TransactionDAO
}

TransactionEditor::TransactionEditor(QWidget* parent)
    : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    setFixedWidth(300);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    // Title and close button
    auto* header = new QHBoxLayout();
    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel, 1);
    auto* closeButton = new QToolButton(this);
    closeButton->setText("✕");
    closeButton->setAutoRaise(true);
    closeButton->setToolTip("Close (Esc)");
    connect(closeButton, &QToolButton::clicked, this, &TransactionEditor::closed);
    header->addWidget(closeButton);
    layout->addLayout(header);

    // Expense / income switch
    auto* typeRow = new QHBoxLayout();
    typeRow->setSpacing(0);
    expenseButton = new QPushButton("Expense", this);
    incomeButton = new QPushButton("Income", this);
    typeGroup = new QButtonGroup(this);
    for (QPushButton* button : {expenseButton, incomeButton}) {
        button->setCheckable(true);
        button->setFocusPolicy(Qt::TabFocus);
        Theme::highlightWhenChecked(button);
        typeGroup->addButton(button);
        typeRow->addWidget(button);
    }
    connect(typeGroup, &QButtonGroup::buttonClicked, this, [this]() {
        populateCategories(categoryCombo->currentData().toInt());
    });
    layout->addLayout(typeRow);

    auto* form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setVerticalSpacing(8);

    amountEdit = new QDoubleSpinBox(this);
    amountEdit->setObjectName("amountEdit");
    amountEdit->setRange(0.0, 999999999.99);
    amountEdit->setDecimals(2);
    amountEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    amountEdit->setAlignment(Qt::AlignRight);
    QFont amountFont = amountEdit->font();
    amountFont.setPointSize(13);
    amountEdit->setFont(amountFont);
    form->addRow("Amount", amountEdit);

    payeeEdit = new QLineEdit(this);
    payeeEdit->setObjectName("payeeEdit");
    payeeEdit->setPlaceholderText("Who did you pay or get paid by?");
    payeeCompleter = new QCompleter(this);
    payeeCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    payeeCompleter->setFilterMode(Qt::MatchContains);
    payeeCompleter->setModel(new QStringListModel(payeeCompleter));
    payeeEdit->setCompleter(payeeCompleter);
    form->addRow("Payee", payeeEdit);

    dateEdit = new QDateEdit(this);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd.MM.yyyy");
    form->addRow("Date", dateEdit);

    categoryCombo = new QComboBox(this);
    form->addRow("Category", categoryCombo);

    accountCombo = new QComboBox(this);
    accountCombo->setObjectName("accountCombo");
    form->addRow("Account", accountCombo);

    descriptionEdit = new QTextEdit(this);
    descriptionEdit->setAcceptRichText(false);
    descriptionEdit->setTabChangesFocus(true);
    descriptionEdit->setPlaceholderText("Notes (optional)");
    descriptionEdit->setMinimumHeight(70);
    descriptionEdit->installEventFilter(this);
    form->addRow("Description", descriptionEdit);
    layout->addLayout(form);

    errorLabel = new QLabel(this);
    errorLabel->setWordWrap(true);
    QPalette errorPalette = errorLabel->palette();
    errorPalette.setColor(QPalette::WindowText, Theme::colors().danger);
    errorLabel->setPalette(errorPalette);
    errorLabel->hide();
    layout->addWidget(errorLabel);

    // Save / cancel, delete for existing transactions
    auto* buttons = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save", this);
    saveButton->setObjectName("saveButton");
    saveButton->setDefault(true);
    saveButton->setToolTip("Save (Enter)");
    connect(saveButton, &QPushButton::clicked, this, &TransactionEditor::requestSave);
    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &TransactionEditor::closed);
    deleteButton = new QPushButton("Delete", this);
    connect(deleteButton, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(transactionId);
    });
    buttons->addWidget(saveButton, 1);
    buttons->addWidget(cancelButton);
    buttons->addStretch();
    buttons->addWidget(deleteButton);
    layout->addLayout(buttons);
    layout->addStretch();

    applySettings();
}

void TransactionEditor::setAccounts(const QList<QPair<int, QString>>& accounts) {
    const int selected = accountCombo->currentData().toInt();
    accountCombo->clear();
    for (const auto& account : accounts) {
        accountCombo->addItem(account.second, account.first);
    }
    selectAccount(selected);
}

void TransactionEditor::setCategories(const QList<Category>& newCategories) {
    categories = newCategories;
    populateCategories(categoryCombo->currentData().toInt());
}

void TransactionEditor::setPayeeSuggestions(const QStringList& payees) {
    static_cast<QStringListModel*>(payeeCompleter->model())->setStringList(payees);
}

void TransactionEditor::applySettings() {
    const bool comma = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma;
    QLocale locale(comma ? QLocale::German : QLocale::English);
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    amountEdit->setLocale(locale);
    amountEdit->setValue(amountEdit->value());       // Re-render with the new separator
}

void TransactionEditor::newTransaction(const QDate& date, int accountId) {
    transactionId = -1;
    titleLabel->setText("New transaction");
    deleteButton->hide();
    errorLabel->hide();

    setType("expense");
    populateCategories(NO_CATEGORY);
    amountEdit->setValue(0.0);
    payeeEdit->clear();
    dateEdit->setDate(date);
    descriptionEdit->clear();
    selectAccount(accountId);

    amountEdit->setFocus();
    amountEdit->selectAll();
}

void TransactionEditor::editTransaction(const Transaction& transaction) {
    transactionId = transaction.getTransactionId();
    titleLabel->setText("Edit transaction");
    deleteButton->show();
    errorLabel->hide();

    setType(transaction.getType());
    populateCategories(transaction.getCategoryId());
    amountEdit->setValue(Money::toDouble(transaction.getAmountCents()));
    payeeEdit->setText(transaction.getPayee());
    dateEdit->setDate(transaction.getDate());
    descriptionEdit->setPlainText(transaction.getDescription());
    selectAccount(transaction.getAccountId());
}

bool TransactionEditor::isEditingNew() const {
    return transactionId == -1;
}

int TransactionEditor::editedTransactionId() const {
    return transactionId;
}

void TransactionEditor::showError(const QString& message) {
    errorLabel->setText(message);
    errorLabel->show();
}

QString TransactionEditor::currentType() const {
    return incomeButton->isChecked() ? QString("income") : QString("expense");
}

void TransactionEditor::setType(const QString& type) {
    (type == "income" ? incomeButton : expenseButton)->setChecked(true);
}

void TransactionEditor::populateCategories(int selectedCategoryId) {
    // Offer the categories matching the transaction type, plus "no category"
    categoryCombo->clear();
    categoryCombo->addItem("(No category)", NO_CATEGORY);
    for (const Category& category : categories) {
        if (category.getType() == currentType() || category.getCategoryId() == selectedCategoryId) {
            categoryCombo->addItem(category.getName(), category.getCategoryId());
        }
    }
    const int index = categoryCombo->findData(selectedCategoryId);
    categoryCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void TransactionEditor::selectAccount(int accountId) {
    // Transactions without an account get a placeholder until one is chosen
    const int placeholder = accountCombo->findData(NO_ACCOUNT);
    if (placeholder >= 0) {
        accountCombo->removeItem(placeholder);
    }
    int index = accountCombo->findData(accountId);
    if (index < 0 && accountId <= 0) {
        accountCombo->insertItem(0, "Choose an account", NO_ACCOUNT);
        index = 0;
    }
    accountCombo->setCurrentIndex(qMax(index, 0));
}

void TransactionEditor::requestSave() {
    const int accountId = accountCombo->currentData().toInt();
    if (accountId == NO_ACCOUNT || accountCombo->count() == 0) {
        showError("Choose an account for this transaction.");
        accountCombo->setFocus();
        return;
    }
    if (Money::fromDouble(amountEdit->value()) <= 0) {
        showError("Enter an amount greater than zero.");
        amountEdit->setFocus();
        amountEdit->selectAll();
        return;
    }
    errorLabel->hide();

    Transaction transaction(transactionId, accountId, payeeEdit->text().trimmed(), Money::fromDouble(amountEdit->value()),
                            dateEdit->date(), descriptionEdit->toPlainText().trimmed(),
                            categoryCombo->currentData().toInt(), currentType());
    emit saveRequested(transaction);
}

void TransactionEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit closed();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        requestSave();
        return;
    }
    QFrame::keyPressEvent(event);
}

bool TransactionEditor::eventFilter(QObject* watched, QEvent* event) {
    // Enter in the description saves too; Shift+Enter adds a line break
    if (watched == descriptionEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool enter = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
        if (enter && !(keyEvent->modifiers() & Qt::ShiftModifier)) {
            requestSave();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closed();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}
