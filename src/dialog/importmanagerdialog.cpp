#include "importmanagerdialog.h"
#include "account.h"
#include "category.h"
#include "csvparser.h"
#include "settings.h"
#include "theme.h"
#include "widgets/categorytag.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

const QLocale english(QLocale::English);

// The fields shown on the Columns step, in order, with their labels
const QList<QPair<QString, QString>> &columnFields()
{
    static const QList<QPair<QString, QString>> fields = {
        {CsvImport::FieldDate, "Date"},
        {CsvImport::FieldAmount, "Amount"},
        {CsvImport::FieldPayee, "Payee"},
        {CsvImport::FieldDescription, "Description"},
        {CsvImport::FieldCategory, "Category"},
        {CsvImport::FieldBalance, "Bank balance"},
    };
    return fields;
}

QString delimiterName(QChar delimiter)
{
    if (delimiter == ';') return "semicolon separated";
    if (delimiter == '\t') return "tab separated";
    if (delimiter == '|') return "pipe separated";
    return "comma separated";
}

void setTextColor(QWidget *widget, const QColor &color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    widget->setPalette(palette);
}

void makePrimary(QPushButton *button)
{
    QPalette palette = button->palette();
    palette.setColor(QPalette::Button, Theme::colors().accent);
    palette.setColor(QPalette::ButtonText, Theme::colors().onAccent);
    button->setPalette(palette);
}

// A small number tile on the review step
QLabel *addTile(QHBoxLayout *layout, const QString &caption, const QString &name, QWidget *parent)
{
    auto *tile = new QFrame(parent);
    tile->setFrameShape(QFrame::StyledPanel);
    auto *tileLayout = new QVBoxLayout(tile);
    tileLayout->setContentsMargins(12, 8, 12, 8);
    tileLayout->setSpacing(2);
    auto *captionLabel = new QLabel(caption, tile);
    captionLabel->setForegroundRole(QPalette::PlaceholderText);
    auto *value = new QLabel("0", tile);
    value->setObjectName(name);
    QFont font = value->font();
    font.setPointSize(14);
    value->setFont(font);
    tileLayout->addWidget(captionLabel);
    tileLayout->addWidget(value);
    layout->addWidget(tile, 1);
    return value;
}

} // namespace

ImportManagerDialog::ImportManagerDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , userId(userId)
{
    setWindowTitle("Import transactions");
    resize(780, 680);
    setMinimumSize(640, 480);
    decimalSeparator = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(14);

    // Where am I: File › Columns › Categories › Review
    auto *steps = new QHBoxLayout();
    steps->setSpacing(8);
    const QStringList stepNames = {"File", "Columns", "Categories", "Review"};
    for (int i = 0; i < stepNames.size(); ++i) {
        if (i > 0) {
            auto *separator = new QLabel("›", this);
            separator->setForegroundRole(QPalette::PlaceholderText);
            steps->addWidget(separator);
        }
        auto *label = new QLabel(stepNames[i], this);
        label->setObjectName("step" + stepNames[i]);
        stepLabels.append(label);
        steps->addWidget(label);
    }
    steps->addStretch();
    root->addLayout(steps);

    pages = new QStackedWidget(this);
    pages->addWidget(buildFilePage());
    pages->addWidget(buildColumnsPage());
    pages->addWidget(buildCategoriesPage());
    pages->addWidget(buildReviewPage());
    root->addWidget(pages, 1);

    auto *buttons = new QHBoxLayout();
    auto *cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancelButton);
    buttons->addStretch();
    backButton = new QPushButton("Back", this);
    backButton->setObjectName("backButton");
    connect(backButton, &QPushButton::clicked, this, &ImportManagerDialog::back);
    buttons->addWidget(backButton);
    nextButton = new QPushButton("Next", this);
    nextButton->setObjectName("nextButton");
    nextButton->setDefault(true);
    makePrimary(nextButton);
    connect(nextButton, &QPushButton::clicked, this, &ImportManagerDialog::next);
    buttons->addWidget(nextButton);
    importButton = new QPushButton("Import", this);
    importButton->setObjectName("importButton");
    makePrimary(importButton);
    connect(importButton, &QPushButton::clicked, this, &ImportManagerDialog::import);
    buttons->addWidget(importButton);
    root->addLayout(buttons);

    goTo(Step::File);
}

ImportManagerDialog::Step ImportManagerDialog::currentStep() const
{
    return static_cast<Step>(pages->currentIndex());
}

QLabel *ImportManagerDialog::pageTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setPointSize(13);
    label->setFont(font);
    return label;
}

QLabel *ImportManagerDialog::hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

// --- Step 1: file, account and the file's format ---

QWidget *ImportManagerDialog::buildFilePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(pageTitle("Choose a file", page));
    layout->addWidget(hintLabel("A CSV export from your bank. Aion reads it first and shows you everything before "
                                "anything is saved.", page));

    auto *fileRow = new QHBoxLayout();
    filePathEdit = new QLineEdit(page);
    filePathEdit->setObjectName("filePathEdit");
    filePathEdit->setReadOnly(true);
    filePathEdit->setPlaceholderText("No file chosen");
    auto *chooseButton = new QPushButton("Choose file…", page);
    chooseButton->setObjectName("chooseFileButton");
    connect(chooseButton, &QPushButton::clicked, this, &ImportManagerDialog::chooseFile);
    fileRow->addWidget(filePathEdit, 1);
    fileRow->addWidget(chooseButton);
    layout->addLayout(fileRow);

    // The detected format in one line, changeable on request
    auto *formatRow = new QHBoxLayout();
    formatSummaryLabel = new QLabel(page);
    formatSummaryLabel->setObjectName("formatSummaryLabel");
    formatSummaryLabel->setForegroundRole(QPalette::PlaceholderText);
    changeFormatButton = new QToolButton(page);
    changeFormatButton->setText("Change");
    changeFormatButton->setAutoRaise(true);
    changeFormatButton->setCheckable(true);
    changeFormatButton->setCursor(Qt::PointingHandCursor);
    QPalette linkPalette = changeFormatButton->palette();
    linkPalette.setColor(QPalette::ButtonText, Theme::colors().accent);
    changeFormatButton->setPalette(linkPalette);
    formatRow->addWidget(formatSummaryLabel);
    formatRow->addWidget(changeFormatButton);
    formatRow->addStretch();
    layout->addLayout(formatRow);

    formatBox = new QFrame(page);
    formatBox->setFrameShape(QFrame::StyledPanel);
    auto *formatForm = new QFormLayout(formatBox);
    delimiterCombo = new QComboBox(formatBox);
    delimiterCombo->setObjectName("delimiterCombo");
    delimiterCombo->addItem("Comma ( , )", QString(","));
    delimiterCombo->addItem("Semicolon ( ; )", QString(";"));
    delimiterCombo->addItem("Tab", QString("\t"));
    delimiterCombo->addItem("Pipe ( | )", QString("|"));
    formatForm->addRow("Columns separated by", delimiterCombo);
    decimalCombo = new QComboBox(formatBox);
    decimalCombo->setObjectName("decimalCombo");
    decimalCombo->addItem("Point (1234.56)", QString("."));
    decimalCombo->addItem("Comma (1234,56)", QString(","));
    formatForm->addRow("Decimal separator", decimalCombo);
    formatBox->hide();
    layout->addWidget(formatBox);
    connect(changeFormatButton, &QToolButton::toggled, formatBox, &QWidget::setVisible);
    connect(delimiterCombo, &QComboBox::currentIndexChanged, this, [this]() {
        delimiter = delimiterCombo->currentData().toString().at(0);
        parseFile();
    });
    connect(decimalCombo, &QComboBox::currentIndexChanged, this, [this]() {
        decimalSeparator = decimalCombo->currentData().toString().at(0);
        updateFormatSummary();
        updateExamples();
    });

    auto *accountForm = new QFormLayout();
    accountForm->setContentsMargins(0, 8, 0, 0);
    accountCombo = new QComboBox(page);
    accountCombo->setObjectName("accountComboBox");
    connect(accountCombo, &QComboBox::currentIndexChanged, this, [this]() {
        expectedEditedByUser = false;
        const bool creating = accountCombo->currentData().toInt() == NewAccount;
        newAccountBox->setVisible(creating);
        if (creating) {
            newAccountNameEdit->setFocus();
        } else {
            lastAccountIndex = accountCombo->currentIndex();
        }
    });
    accountForm->addRow("Import into", accountCombo);
    layout->addLayout(accountForm);

    // A new account right here. No starting balance: the balance comes from the imported
    // transactions, and the review's balance check offers a correction if the bank's differs.
    newAccountBox = new QFrame(page);
    newAccountBox->setObjectName("newAccountBox");
    newAccountBox->setFrameShape(QFrame::StyledPanel);
    auto *newAccountLayout = new QVBoxLayout(newAccountBox);
    auto *newAccountForm = new QFormLayout();
    newAccountNameEdit = new QLineEdit(newAccountBox);
    newAccountNameEdit->setObjectName("newAccountNameEdit");
    newAccountNameEdit->setPlaceholderText("For example Girokonto");
    connect(newAccountNameEdit, &QLineEdit::returnPressed, this, &ImportManagerDialog::createAccount);
    newAccountForm->addRow("Name", newAccountNameEdit);
    newAccountTypeCombo = new QComboBox(newAccountBox);
    newAccountTypeCombo->setObjectName("newAccountTypeCombo");
    newAccountTypeCombo->addItems(AccountDAO().getAccountTypes());
    newAccountForm->addRow("Type", newAccountTypeCombo);
    newAccountLayout->addLayout(newAccountForm);
    newAccountLayout->addWidget(hintLabel("The balance comes from the imported transactions. If your bank shows a "
                                          "different one, the last step can correct it.", newAccountBox));
    auto *newAccountButtons = new QHBoxLayout();
    newAccountButtons->addStretch();
    auto *cancelNewAccount = new QPushButton("Cancel", newAccountBox);
    connect(cancelNewAccount, &QPushButton::clicked, this, [this]() {
        if (lastAccountIndex >= 0) accountCombo->setCurrentIndex(lastAccountIndex);   // No account yet: keep the form
    });
    newAccountButtons->addWidget(cancelNewAccount);
    auto *createAccountButton = new QPushButton("Create account", newAccountBox);
    createAccountButton->setObjectName("createAccountButton");
    connect(createAccountButton, &QPushButton::clicked, this, &ImportManagerDialog::createAccount);
    newAccountButtons->addWidget(createAccountButton);
    newAccountLayout->addLayout(newAccountButtons);
    newAccountBox->hide();
    layout->addWidget(newAccountBox);
    loadAccounts(-1);

    fileErrorLabel = new QLabel(page);
    fileErrorLabel->setWordWrap(true);
    fileErrorLabel->hide();
    layout->addWidget(fileErrorLabel);
    layout->addStretch();

    updateFormatSummary();
    return page;
}

void ImportManagerDialog::loadAccounts(int selectAccountId)
{
    const QSignalBlocker blocker(accountCombo);
    accountCombo->clear();
    for (const Account &account : AccountDAO().getAllAccountsForUser(userId)) {
        accountCombo->addItem(account.getName(), account.getAccountId());
    }
    accountCombo->insertSeparator(accountCombo->count());
    accountCombo->addItem("New account…", NewAccount);
    const int index = accountCombo->findData(selectAccountId);
    // Without any account, start right with a new one
    accountCombo->setCurrentIndex(index >= 0 ? index : accountCombo->count() == 2 ? 1 : 0);
    lastAccountIndex = accountCombo->currentData().toInt() == NewAccount ? -1 : accountCombo->currentIndex();
    newAccountBox->setVisible(accountCombo->currentData().toInt() == NewAccount);
}

void ImportManagerDialog::createAccount()
{
    const QString name = newAccountNameEdit->text().simplified();
    if (name.isEmpty()) {
        showError(fileErrorLabel, "Enter a name for the new account.");
        newAccountNameEdit->setFocus();
        return;
    }
    for (int i = 0; i < accountCombo->count(); ++i) {
        if (accountCombo->itemData(i).toInt() > 0 && accountCombo->itemText(i).compare(name, Qt::CaseInsensitive) == 0) {
            showError(fileErrorLabel, QString("You already have an account called \"%1\".").arg(accountCombo->itemText(i)));
            return;
        }
    }
    const int accountId = AccountDAO::createAccount(userId, name, newAccountTypeCombo->currentText(), 0);
    if (accountId <= 0) {
        showError(fileErrorLabel, "The account couldn't be created.");
        return;
    }
    fileErrorLabel->hide();
    newAccountNameEdit->clear();
    loadAccounts(accountId);
    emit accountCreated(accountId);
}

void ImportManagerDialog::chooseFile()
{
    const QString chosen = QFileDialog::getOpenFileName(this, "Choose a file to import", QDir::homePath(),
                                                        "CSV files (*.csv);;All files (*.*)");
    if (!chosen.isEmpty()) {
        loadFile(chosen);
    }
}

void ImportManagerDialog::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showError(fileErrorLabel, QString("The file couldn't be opened: %1").arg(file.errorString()));
        return;
    }
    filePath = path;
    fileText = QTextStream(&file).readAll();
    filePathEdit->setText(QDir::toNativeSeparators(path));
    expectedEditedByUser = false;

    // Detect the delimiter (or use the default from Settings), then read the rows
    const Settings &settings = Settings::instance();
    delimiter = settings.isAutoDetectCsvDelimiter() ? CsvImport::detectDelimiter(fileText) : settings.getDefaultCsvDelimiter();
    {
        const QSignalBlocker blocker(delimiterCombo);
        delimiterCombo->setCurrentIndex(qMax(0, delimiterCombo->findData(QString(delimiter))));
    }
    parseFile();

    // The decimal separator follows the amount column if detection is on
    const QChar appSeparator = settings.getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
    decimalSeparator = settings.isAutoDetectDecimalSeparator()
        ? CsvImport::detectDecimalSeparator(rows, mapping().value(CsvImport::FieldAmount, -1), appSeparator)
        : appSeparator;
    {
        const QSignalBlocker blocker(decimalCombo);
        decimalCombo->setCurrentIndex(qMax(0, decimalCombo->findData(QString(decimalSeparator))));
    }
    updateFormatSummary();
    updateExamples();
}

void ImportManagerDialog::parseFile()
{
    rows.clear();
    headers.clear();
    QString parseError;
    const QList<QStringList> records = CsvParser::parse(fileText, delimiter, &parseError);
    if (!parseError.isEmpty()) {
        showError(fileErrorLabel, QString("This file can't be read as CSV: %1").arg(parseError));
    } else {
        fileErrorLabel->hide();
    }
    if (!records.isEmpty()) {
        headers = records.first();
        rows = records.mid(1);
    }

    // Offer the file's columns and guess which is which from the headers
    for (QComboBox *combo : std::as_const(mappingCombos)) {
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem("(Not in the file)", -1);
        for (int i = 0; i < headers.size(); ++i) combo->addItem(headers[i], i);
    }
    applyMapping(CsvImport::autoMapColumns(headers));
    updateFormatSummary();
    updateExamples();
}

void ImportManagerDialog::updateFormatSummary()
{
    if (filePath.isEmpty()) {
        formatSummaryLabel->setText("Choose a file to see what Aion finds in it.");
        changeFormatButton->hide();
        return;
    }
    formatSummaryLabel->setText(QString("%1 %2 · %3 · decimal %4")
                                    .arg(rows.size()).arg(rows.size() == 1 ? "row" : "rows")
                                    .arg(delimiterName(delimiter), decimalSeparator == ',' ? "comma" : "point"));
    changeFormatButton->show();
    if (columnsSubtitle) {
        columnsSubtitle->setText(QFileInfo(filePath).fileName() + " · " + formatSummaryLabel->text());
    }
}

// --- Step 2: which column is what ---

QWidget *ImportManagerDialog::buildColumnsPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(pageTitle("Which column is what?", page));
    columnsSubtitle = hintLabel(QString(), page);
    layout->addWidget(columnsSubtitle);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    grid->setColumnMinimumWidth(0, 110);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 3);
    int column = 0;
    for (const QString &caption : {QString("Aion field"), QString("Column in the file"), QString("Example")}) {
        auto *header = new QLabel(caption, page);
        header->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(header, 0, column++);
    }
    int row = 1;
    for (const auto &field : columnFields()) {
        auto *label = new QLabel(field.second, page);
        if (field.first == CsvImport::FieldBalance) {
            label->setToolTip("The bank's balance after each booking. Only used to check the account's balance, never saved.");
        }
        auto *combo = new QComboBox(page);
        combo->setObjectName("map_" + field.first);
        auto *example = new QLabel(page);
        example->setObjectName("example_" + field.first);
        example->setForegroundRole(QPalette::PlaceholderText);
        connect(combo, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::updateExamples);
        mappingCombos.insert(field.first, combo);
        exampleLabels.insert(field.first, example);
        grid->addWidget(label, row, 0);
        grid->addWidget(combo, row, 1);
        grid->addWidget(example, row, 2);
        ++row;
    }
    layout->addLayout(grid);

    // Rarely needed: where income and expenses come from, duplicates and unreadable rows
    moreOptionsButton = new QToolButton(page);
    moreOptionsButton->setObjectName("moreOptionsButton");
    moreOptionsButton->setText("More options");
    moreOptionsButton->setCheckable(true);
    moreOptionsButton->setAutoRaise(true);
    moreOptionsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    moreOptionsButton->setArrowType(Qt::RightArrow);
    connect(moreOptionsButton, &QToolButton::toggled, this, [this](bool open) {
        moreOptionsBox->setVisible(open);
        moreOptionsButton->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        if (open) {
            // The explanations wrap: grow the window if they need more room than it has
            this->layout()->activate();
            const int needed = this->layout()->hasHeightForWidth() ? this->layout()->totalHeightForWidth(width()) : sizeHint().height();
            if (needed > height()) resize(width(), needed);
        }
    });
    layout->addWidget(moreOptionsButton);

    moreOptionsBox = new QFrame(page);
    moreOptionsBox->setFrameShape(QFrame::StyledPanel);
    auto *more = new QFormLayout(moreOptionsBox);
    interpretationCombo = new QComboBox(moreOptionsBox);
    interpretationCombo->setObjectName("interpretationCombo");
    interpretationCombo->addItem("From the sign (− expense, + income)", int(CsvImport::AmountInterpretation::SignBased));
    interpretationCombo->addItem("From the sign, reversed (− income, + expense)", int(CsvImport::AmountInterpretation::SignBasedInverted));
    interpretationCombo->addItem("Everything is income", int(CsvImport::AmountInterpretation::AllIncome));
    interpretationCombo->addItem("Everything is an expense", int(CsvImport::AmountInterpretation::AllExpense));
    interpretationCombo->addItem("From a type column", int(CsvImport::AmountInterpretation::ColumnBased));
    connect(interpretationCombo, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::updateExamples);
    more->addRow("Income or expense", interpretationCombo);
    auto *typeCombo = new QComboBox(moreOptionsBox);
    typeCombo->setObjectName("map_" + CsvImport::FieldType);
    typeCombo->setToolTip("A column saying whether a row is income or expense, for example \"Soll/Haben\"");
    connect(typeCombo, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::updateExamples);
    mappingCombos.insert(CsvImport::FieldType, typeCombo);
    more->addRow("Type column", typeCombo);
    // Each option with a line saying when you'd want it
    auto addOption = [&](QCheckBox *check, const QString &explanation) {
        auto *hint = hintLabel(explanation, moreOptionsBox);
        hint->setContentsMargins(24, 0, 0, 4);       // Under the checkbox text
        more->addRow(check);
        more->addRow(hint);
    };
    allowDuplicatesCheck = new QCheckBox("Import identical transactions", moreOptionsBox);
    allowDuplicatesCheck->setObjectName("allowDuplicatesCheck");
    addOption(allowDuplicatesCheck, "Normally rows with the same date, amount, payee and description as a transaction "
                                    "in this account are skipped, so importing overlapping exports doesn't count them twice. "
                                    "Turn this on if you really have identical transactions, like two coffees on one day.");
    allowPartialCheck = new QCheckBox("Skip rows that can't be read", moreOptionsBox);
    allowPartialCheck->setObjectName("allowPartialCheck");
    addOption(allowPartialCheck, "Normally one unreadable row, like a broken date, stops the whole import so nothing is "
                                 "half imported. Turn this on to leave such rows out and import the rest. The review "
                                 "lists the rows that would be left out.");
    moreOptionsBox->hide();
    layout->addWidget(moreOptionsBox);

    columnsErrorLabel = new QLabel(page);
    columnsErrorLabel->setWordWrap(true);
    columnsErrorLabel->hide();
    layout->addWidget(columnsErrorLabel);
    layout->addStretch();
    return page;
}

void ImportManagerDialog::applyMapping(const CsvImport::Mapping &newMapping)
{
    for (auto it = mappingCombos.cbegin(); it != mappingCombos.cend(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setCurrentIndex(qMax(0, it.value()->findData(newMapping.value(it.key(), -1))));
    }
}

CsvImport::Mapping ImportManagerDialog::mapping() const
{
    CsvImport::Mapping result;
    for (auto it = mappingCombos.cbegin(); it != mappingCombos.cend(); ++it) {
        const int column = it.value()->currentData().toInt();
        if (column >= 0) result.insert(it.key(), column);
    }
    return result;
}

void ImportManagerDialog::updateExamples()
{
    const CsvImport::Mapping current = mapping();
    const Settings &settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    const auto interpretation = static_cast<CsvImport::AmountInterpretation>(interpretationCombo->currentData().toInt());

    for (auto it = exampleLabels.cbegin(); it != exampleLabels.cend(); ++it) {
        QLabel *example = it.value();
        example->setForegroundRole(QPalette::PlaceholderText);
        example->setPalette(QPalette());
        const int column = current.value(it.key(), -1);
        // The first row that has something in this column
        QString value;
        const QStringList *sampleRow = nullptr;
        for (const QStringList &row : std::as_const(rows)) {
            if (column >= 0 && column < row.size() && !row[column].trimmed().isEmpty()) {
                value = row[column].trimmed();
                sampleRow = &row;
                break;
            }
        }
        if (column < 0 || value.isEmpty()) {
            example->setText(column < 0 ? QString() : QString("empty in this file"));
            continue;
        }

        QString text = value;
        bool problem = false;
        if (it.key() == CsvImport::FieldDate) {
            const QDate date = CsvImport::parseDate(value);
            problem = !date.isValid();
            text = problem ? QString("can't read \"%1\" as a date").arg(value) : english.toString(date, "d MMMM yyyy");
        } else if (it.key() == CsvImport::FieldAmount || it.key() == CsvImport::FieldBalance) {
            bool ok = false;
            const Cents cents = CsvImport::parseAmount(value, decimalSeparator, &ok);
            problem = !ok;
            if (problem) {
                text = QString("can't read \"%1\" as an amount").arg(value);
            } else if (it.key() == CsvImport::FieldBalance) {
                text = settings.formatCurrency(cents) + " · only for checking the balance";
            } else {
                const int typeColumn = current.value(CsvImport::FieldType, -1);
                const QString typeText = typeColumn >= 0 && typeColumn < sampleRow->size() ? sampleRow->at(typeColumn) : QString();
                const QString type = CsvImport::transactionType(cents, typeText, interpretation);
                text = QString("%1 → %2 %3").arg(value, type, settings.formatCurrency(qAbs(cents)));
            }
        }
        example->setText(example->fontMetrics().elidedText(text, Qt::ElideRight, 320));
        example->setToolTip(text);
        if (problem) setTextColor(example, colors.danger);
    }
}

CsvImport::Options ImportManagerDialog::importOptions() const
{
    CsvImport::Options options;
    options.accountId = accountCombo->currentData().toInt();
    options.decimalSeparator = decimalSeparator;
    options.interpretation = static_cast<CsvImport::AmountInterpretation>(interpretationCombo->currentData().toInt());
    for (const Category &category : CategoryDAO().getAllCategories()) {
        options.categoryIds.insert(category.getName().simplified().toLower(), category.getCategoryId());
    }
    applyCategoryChoices(options);
    return options;
}

// --- Step 3: the file's categories ---

QWidget *ImportManagerDialog::buildCategoriesPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(pageTitle("Categories in this file", page));

    auto *summaryRow = new QHBoxLayout();
    categorySummaryLabel = new QLabel(page);
    categorySummaryLabel->setObjectName("categorySummaryLabel");
    categorySummaryLabel->setWordWrap(true);
    categorySummaryLabel->setForegroundRole(QPalette::PlaceholderText);
    summaryRow->addWidget(categorySummaryLabel, 1);
    newCategoriesButton = new QToolButton(page);
    newCategoriesButton->setObjectName("newCategoriesButton");
    newCategoriesButton->setText("All new categories");
    newCategoriesButton->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(newCategoriesButton);
    menu->addAction("Create them", this, [this]() { setAllNewCategories(true); });
    menu->addAction("Import without a category", this, [this]() { setAllNewCategories(false); });
    newCategoriesButton->setMenu(menu);
    summaryRow->addWidget(newCategoriesButton);
    layout->addLayout(summaryRow);

    categoriesScroll = new QScrollArea(page);
    categoriesScroll->setObjectName("categoriesScroll");
    categoriesScroll->setWidgetResizable(true);
    categoriesScroll->setFrameShape(QFrame::StyledPanel);
    categoriesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(categoriesScroll, 1);
    return page;
}

void ImportManagerDialog::refreshFileCategories()
{
    // Keep the user's choices while the list is rebuilt
    for (auto it = categoryChoices.cbegin(); it != categoryChoices.cend(); ++it) {
        chosenCategories.insert(it.key(), it.value()->currentData().toString());
    }
    categoryChoices.clear();
    delete categoriesScroll->takeWidget();

    CsvImport::Options options;
    options.decimalSeparator = decimalSeparator;
    options.interpretation = static_cast<CsvImport::AmountInterpretation>(interpretationCombo->currentData().toInt());
    fileCategories = CsvImport::fileCategories(rows, mapping(), options);

    QHash<QString, Category> existing;
    QList<Category> choosable;
    for (const Category &category : CategoryDAO().getAllCategories()) {
        existing.insert(category.getName().simplified().toLower(), category);
        if (!category.isBuiltIn()) choosable.append(category);
    }

    auto *table = new QWidget();
    auto *grid = new QGridLayout(table);
    grid->setContentsMargins(10, 8, 10, 8);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 2);
    int column = 0;
    for (const QString &caption : {QString("In the file"), QString("Rows"), QString("In Aion")}) {
        auto *header = new QLabel(caption, table);
        header->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(header, 0, column++);
    }
    int row = 1;
    for (const CsvImport::FileCategory &category : std::as_const(fileCategories)) {
        const QString key = category.name.toLower();
        grid->addWidget(new QLabel(category.name, table), row, 0);
        auto *count = new QLabel(QString::number(category.rows), table);
        count->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(count, row, 1);

        if (existing.contains(key)) {
            // Already known: just show it
            const Category &known = existing.find(key).value();
            auto *match = new QWidget(table);
            auto *matchLayout = new QHBoxLayout(match);
            matchLayout->setContentsMargins(0, 0, 0, 0);
            auto *tag = new CategoryTag(match);
            tag->setCategory(known.getName(), known.getColor());
            matchLayout->addWidget(tag);
            auto *note = new QLabel("matches", match);
            note->setForegroundRole(QPalette::PlaceholderText);
            matchLayout->addWidget(note);
            matchLayout->addStretch();
            grid->addWidget(match, row, 2);
        } else {
            auto *choice = new QComboBox(table);
            choice->setObjectName("categoryChoice");
            choice->setAccessibleName(category.name);
            choice->setProperty("fileName", category.name);
            const QString likely = category.likelyType();
            const QString other = likely == "expense" ? "income" : "expense";
            choice->addItem(QString("Create \"%1\" (%2)").arg(category.name, likely), "create:" + likely);
            choice->addItem(QString("Create \"%1\" (%2)").arg(category.name, other), "create:" + other);
            choice->addItem("No category", "none");
            choice->insertSeparator(choice->count());
            for (const Category &target : choosable) {
                choice->addItem(target.getName(), QString("id:%1").arg(target.getCategoryId()));
            }
            const int remembered = choice->findData(chosenCategories.value(key));
            choice->setCurrentIndex(remembered >= 0 ? remembered : 0);
            connect(choice, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::updateCategorySummary);
            categoryChoices.insert(key, choice);
            grid->addWidget(choice, row, 2);
        }
        ++row;
    }
    grid->setRowStretch(row, 1);
    matchedCategories = int(fileCategories.size() - categoryChoices.size());
    categoriesScroll->setWidget(table);
    updateCategorySummary();
}

void ImportManagerDialog::updateCategorySummary()
{
    int create = 0;
    int none = 0;
    for (QComboBox *choice : std::as_const(categoryChoices)) {
        const QString choiceData = choice->currentData().toString();
        if (choiceData.startsWith("create:")) ++create;
        else if (choiceData == "none") ++none;
    }
    const int mapped = int(categoryChoices.size()) - create - none;
    QStringList parts;
    if (matchedCategories > 0) parts << QString("%1 already in Aion").arg(matchedCategories);
    if (create > 0) parts << QString("%1 will be created").arg(create);
    if (mapped > 0) parts << QString("%1 mapped to other categories").arg(mapped);
    if (none > 0) parts << QString("%1 imported without a category").arg(none);
    const int total = matchedCategories + int(categoryChoices.size());
    categorySummaryLabel->setText(QString("%1 %2 in the file: %3.")
                                      .arg(total).arg(total == 1 ? "category" : "categories", parts.join(", ")));
    newCategoriesButton->setVisible(!categoryChoices.isEmpty());
}

void ImportManagerDialog::setAllNewCategories(bool create)
{
    for (QComboBox *choice : std::as_const(categoryChoices)) {
        const QSignalBlocker blocker(choice);
        choice->setCurrentIndex(create ? 0 : choice->findData("none"));   // Index 0: create with the likely type
    }
    updateCategorySummary();
}

void ImportManagerDialog::applyCategoryChoices(CsvImport::Options &options) const
{
    for (auto it = categoryChoices.cbegin(); it != categoryChoices.cend(); ++it) {
        const QString choice = it.value()->currentData().toString();
        if (choice.startsWith("create:")) {
            options.categoriesToCreate.insert(it.key(), {it.value()->property("fileName").toString(), choice.mid(7)});
        } else if (choice.startsWith("id:")) {
            options.categoryIds.insert(it.key(), choice.mid(3).toInt());
        } else {
            options.categoryIds.remove(it.key());   // No category
        }
    }
}

// --- Step 4: review and balance check ---

QWidget *ImportManagerDialog::buildReviewPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    reviewTitle = pageTitle(QString(), page);
    layout->addWidget(reviewTitle);
    layout->addWidget(hintLabel("Nothing has been saved yet.", page));

    auto *tiles = new QHBoxLayout();
    tiles->setSpacing(8);
    newCountLabel = addTile(tiles, "New", "newCountLabel", page);
    duplicateCountLabel = addTile(tiles, "Already in Aion, skipped", "duplicateCountLabel", page);
    failedCountLabel = addTile(tiles, "Can't be read", "failedCountLabel", page);
    layout->addLayout(tiles);

    problemsLabel = new QLabel(page);
    problemsLabel->setObjectName("problemsLabel");
    problemsLabel->setWordWrap(true);
    layout->addWidget(problemsLabel);
    newCategoriesLabel = new QLabel(page);
    newCategoriesLabel->setObjectName("newCategoriesLabel");
    newCategoriesLabel->setWordWrap(true);
    layout->addWidget(newCategoriesLabel);

    // Balance after the import compared with the bank's
    balanceBox = new QFrame(page);
    balanceBox->setObjectName("balanceGroupBox");
    balanceBox->setFrameShape(QFrame::StyledPanel);
    auto *grid = new QGridLayout(balanceBox);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setColumnStretch(0, 1);
    balanceAfterCaption = new QLabel(balanceBox);
    balanceAfterValue = new QLabel(balanceBox);
    balanceAfterValue->setObjectName("balanceAfterValue");
    balanceAfterValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont bold = balanceAfterValue->font();
    bold.setBold(true);
    balanceAfterValue->setFont(bold);
    grid->addWidget(balanceAfterCaption, 0, 0);
    grid->addWidget(balanceAfterValue, 0, 1);
    balanceBreakdownLabel = new QLabel(balanceBox);
    balanceBreakdownLabel->setObjectName("balanceBreakdownLabel");
    balanceBreakdownLabel->setForegroundRole(QPalette::PlaceholderText);
    grid->addWidget(balanceBreakdownLabel, 1, 0, 1, 2);
    expectedCaption = new QLabel(balanceBox);
    expectedEdit = new QLineEdit(balanceBox);
    expectedEdit->setObjectName("expectedBalanceEdit");
    expectedEdit->setPlaceholderText("Optional");
    expectedEdit->setAlignment(Qt::AlignRight);
    expectedEdit->setMaximumWidth(160);
    connect(expectedEdit, &QLineEdit::textEdited, this, [this]() {
        expectedEditedByUser = true;
        expectedSourceLabel->setText(" ");
        updateBalanceStatus();
    });
    grid->addWidget(expectedCaption, 2, 0);
    grid->addWidget(expectedEdit, 2, 1);
    expectedSourceLabel = new QLabel(" ", balanceBox);
    expectedSourceLabel->setForegroundRole(QPalette::PlaceholderText);
    grid->addWidget(expectedSourceLabel, 3, 0, 1, 2);
    balanceStatusLabel = new QLabel(balanceBox);
    balanceStatusLabel->setObjectName("balanceStatusLabel");
    balanceStatusLabel->setWordWrap(true);
    grid->addWidget(balanceStatusLabel, 4, 0, 1, 2);
    correctionCheckBox = new QCheckBox(balanceBox);
    correctionCheckBox->setObjectName("correctionCheckBox");
    correctionCheckBox->setChecked(true);
    connect(correctionCheckBox, &QCheckBox::toggled, this, &ImportManagerDialog::updateBalanceStatus);
    grid->addWidget(correctionCheckBox, 5, 0, 1, 2);
    layout->addWidget(balanceBox);
    layout->addStretch();
    return page;
}

void ImportManagerDialog::refreshReview()
{
    dryRun = runImport(true);
    reviewTitle->setText(QString("Ready to import into %1").arg(accountCombo->currentText()));
    newCountLabel->setText(QString::number(dryRun.imported));
    duplicateCountLabel->setText(QString::number(dryRun.duplicates));
    failedCountLabel->setText(QString::number(dryRun.failed));
    setTextColor(failedCountLabel, dryRun.failed > 0 ? Theme::colors().danger : palette().color(QPalette::WindowText));

    // Rows that can't be read: say why and what to do
    if (dryRun.failed > 0) {
        QStringList shown = dryRun.errors.mid(0, 4);
        QString text = shown.join("\n");
        if (dryRun.errors.size() > shown.size()) text += QString("\n… and %1 more").arg(dryRun.errors.size() - shown.size());
        if (!allowPartialCheck->isChecked()) {
            text += "\n\nNothing will be imported like this. Check the columns, or go back and choose "
                    "\"Skip rows that can't be read\" under More options.";
        }
        problemsLabel->setText(text);
        setTextColor(problemsLabel, Theme::colors().danger);
        problemsLabel->show();
    } else if (dryRun.imported == 0 && dryRun.duplicates > 0) {
        problemsLabel->setText("Everything in this file is already in Aion.");
        problemsLabel->setPalette(QPalette());
        problemsLabel->show();
    } else {
        problemsLabel->hide();
    }

    QStringList created;
    for (QComboBox *choice : std::as_const(categoryChoices)) {
        if (choice->currentData().toString().startsWith("create:")) created << choice->property("fileName").toString();
    }
    std::sort(created.begin(), created.end(), [](const QString &a, const QString &b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
    newCategoriesLabel->setText(created.isEmpty() ? QString()
        : created.size() <= 3 ? QString("New categories: %1").arg(created.join(", "))
                              : QString("New categories: %1 and %2 more").arg(created.mid(0, 2).join(", ")).arg(created.size() - 2));
    newCategoriesLabel->setVisible(!created.isEmpty());

    // Balance check on the newest day of the file (or the day of the bank's balance column)
    balanceBox->setVisible(dryRun.checkDate.isValid());
    if (dryRun.checkDate.isValid()) {
        const Settings &settings = Settings::instance();
        const QString day = english.toString(dryRun.checkDate, "d MMMM yyyy");
        balanceAfterCaption->setText(QString("Balance on %1 after the import").arg(day));
        balanceAfterValue->setText(settings.formatCurrency(dryRun.balanceAfterCents));
        balanceBreakdownLabel->setText(QString("Now %1 · from this file %2")
                                           .arg(settings.formatCurrency(dryRun.balanceBeforeCents),
                                                settings.formatCurrency(dryRun.balanceAfterCents - dryRun.balanceBeforeCents, true)));
        expectedCaption->setText("Your bank says");
        if (!expectedEditedByUser) {
            const CsvImport::StatedBalance stated = CsvImport::statedBalance(rows, mapping(), decimalSeparator);
            expectedEdit->setText(stated.found ? settings.formatCurrency(stated.cents) : QString());
            expectedSourceLabel->setText(stated.found ? QString("From the file's \"%1\" column").arg(headers.value(mapping().value(CsvImport::FieldBalance)))
                                                      : QString(" "));
        }
    }
    updateBalanceStatus();
}

bool ImportManagerDialog::expectedBalance(Cents *cents) const
{
    if (expectedEdit->text().trimmed().isEmpty()) {
        return false;
    }
    const QChar separator = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
    bool ok = false;
    *cents = CsvImport::parseAmount(expectedEdit->text(), separator, &ok);
    return ok;
}

void ImportManagerDialog::updateBalanceStatus()
{
    const Settings &settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    Cents expected = 0;
    bool correction = false;
    if (!dryRun.checkDate.isValid()) {
        balanceStatusLabel->clear();
        correctionCheckBox->hide();
    } else if (expectedEdit->text().trimmed().isEmpty()) {
        balanceStatusLabel->setText("Enter the balance your bank shows for that day to compare.");
        setTextColor(balanceStatusLabel, colors.textSecondary);
        correctionCheckBox->hide();
    } else if (!expectedBalance(&expected)) {
        balanceStatusLabel->setText("That isn't an amount Aion can read.");
        setTextColor(balanceStatusLabel, colors.danger);
        correctionCheckBox->hide();
    } else if (expected == dryRun.balanceAfterCents) {
        balanceStatusLabel->setText("Matches your bank after the import.");
        setTextColor(balanceStatusLabel, colors.positive);
        correctionCheckBox->hide();
    } else {
        const Cents difference = expected - dryRun.balanceAfterCents;
        const QString amount = settings.formatCurrency(qAbs(difference));
        const QString direction = difference > 0 ? "less" : "more";
        if (dryRun.imported == 0) {
            // The file adds nothing, so the difference was there before this import
            balanceStatusLabel->setText(QString("Aion already shows %1 %2 than your bank on that day. This file adds "
                                                "nothing, so the difference comes from earlier: for example transactions "
                                                "missing in Aion, or a starting balance that doesn't fit the imported history.")
                                            .arg(amount, direction));
        } else {
            balanceStatusLabel->setText(QString("%1 %2 than your bank after the import. Missing or extra bookings, for example "
                                                "from a gap between exports, can cause this.")
                                            .arg(amount, direction));
        }
        setTextColor(balanceStatusLabel, colors.danger);
        correctionCheckBox->setText(QString("Add a balance correction of %1 so they match")
                                        .arg(settings.formatCurrency(difference, true)));
        correctionCheckBox->show();
        correction = correctionCheckBox->isChecked();
    }

    // Import what can be imported; a correction alone is fine too. With nothing to do (everything
    // is already in Aion) the wizard can still be finished.
    const bool blockedByProblems = dryRun.failed > 0 && !allowPartialCheck->isChecked();
    const bool rowsToImport = dryRun.imported > 0 && !blockedByProblems;
    closeOnly = !rowsToImport && !correction && !blockedByProblems;
    if (rowsToImport) {
        importButton->setText(QString("Import %1 %2").arg(dryRun.imported).arg(dryRun.imported == 1 ? "transaction" : "transactions"));
    } else if (correction) {
        importButton->setText("Add balance correction");
    } else {
        importButton->setText(closeOnly ? "Close" : "Import");
    }
    importButton->setEnabled(rowsToImport || correction || closeOnly);
}

CsvImport::Result ImportManagerDialog::runImport(bool dry)
{
    const CsvImport::Options options = importOptions();
    CsvImport::RunOptions runOptions;
    runOptions.allowDuplicates = allowDuplicatesCheck->isChecked();
    // The review counts rows one by one; whether unreadable rows stop the import is shown separately
    runOptions.allowPartialImport = dry || allowPartialCheck->isChecked();
    runOptions.dryRun = dry;

    // Check the balance on the day the bank's balance column refers to, if it is mapped
    const CsvImport::StatedBalance stated = CsvImport::statedBalance(rows, mapping(), decimalSeparator);
    if (stated.found) {
        runOptions.checkDate = stated.date;
    }
    Cents expected = 0;
    if (!dry && correctionCheckBox->isVisible() && correctionCheckBox->isChecked() && expectedBalance(&expected)) {
        runOptions.correct = true;
        runOptions.correctTo = expected;
        runOptions.correctionNote = QString("Import of %1").arg(QFileInfo(filePath).fileName());
    }
    return CsvImport::run(rows, mapping(), options, runOptions);
}

void ImportManagerDialog::import()
{
    if (closeOnly) {
        accept();                                    // Nothing new and no correction wanted
        return;
    }
    const CsvImport::Result result = runImport(false);
    if (result.imported == 0 && result.correctionCents == 0) {
        QMessageBox::warning(this, "Nothing imported",
                             result.errors.isEmpty() ? QString("There was nothing new to import.") : result.errors.join("\n"));
        refreshReview();
        return;
    }

    QStringList lines;
    lines << QString("%1 %2 imported.").arg(result.imported).arg(result.imported == 1 ? "transaction" : "transactions");
    if (result.duplicates > 0) lines << QString("%1 already in Aion, skipped.").arg(result.duplicates);
    if (result.categoriesCreated > 0) lines << QString("%1 new %2.").arg(result.categoriesCreated).arg(result.categoriesCreated == 1 ? "category" : "categories");
    if (result.correctionCents != 0) {
        lines << QString("Balance correction of %1 on %2.").arg(Settings::instance().formatCurrency(result.correctionCents, true),
                                                                english.toString(result.checkDate, "d MMMM yyyy"));
    }
    if (result.failed > 0) lines << QString("%1 %2 couldn't be read.").arg(result.failed).arg(result.failed == 1 ? "row" : "rows");
    QMessageBox::information(this, "Import complete", lines.join("\n"));
    emit transactionsImported(result.imported);
    accept();
}

// --- Navigation ---

bool ImportManagerDialog::hasCategoriesStep() const
{
    return !fileCategories.isEmpty();
}

void ImportManagerDialog::showError(QLabel *label, const QString &message)
{
    label->setText(message);
    setTextColor(label, Theme::colors().danger);
    label->setVisible(!message.isEmpty());
}

void ImportManagerDialog::next()
{
    switch (currentStep()) {
    case Step::File:
        if (rows.isEmpty()) {
            showError(fileErrorLabel, filePath.isEmpty() ? QString("Choose a file first.") : QString("This file has no rows to import."));
            return;
        }
        if (accountCombo->currentData().toInt() <= 0) {
            showError(fileErrorLabel, "Create the new account first, or choose an existing one.");
            return;
        }
        fileErrorLabel->hide();
        goTo(Step::Columns);
        break;
    case Step::Columns:
        if (!mapping().contains(CsvImport::FieldDate) || !mapping().contains(CsvImport::FieldAmount)) {
            showError(columnsErrorLabel, "Choose the columns for the date and the amount. Everything else is optional.");
            return;
        }
        columnsErrorLabel->hide();
        refreshFileCategories();
        goTo(hasCategoriesStep() ? Step::Categories : Step::Review);
        break;
    case Step::Categories:
        goTo(Step::Review);
        break;
    case Step::Review:
        break;
    }
}

void ImportManagerDialog::back()
{
    switch (currentStep()) {
    case Step::Columns: goTo(Step::File); break;
    case Step::Categories: goTo(Step::Columns); break;
    case Step::Review: goTo(hasCategoriesStep() ? Step::Categories : Step::Columns); break;
    case Step::File: break;
    }
}

void ImportManagerDialog::goTo(Step step)
{
    if (step == Step::Review) {
        refreshReview();
    }
    pages->setCurrentIndex(static_cast<int>(step));
    backButton->setVisible(step != Step::File);
    nextButton->setVisible(step != Step::Review);
    importButton->setVisible(step == Step::Review);
    (step == Step::Review ? importButton : nextButton)->setDefault(true);
    updateStepIndicator();
}

void ImportManagerDialog::updateStepIndicator()
{
    const Theme::Colors colors = Theme::colors();
    const int current = pages->currentIndex();
    const QStringList names = {"File", "Columns", "Categories", "Review"};
    for (int i = 0; i < stepLabels.size(); ++i) {
        QLabel *label = stepLabels[i];
        const bool skipped = i == int(Step::Categories) && current > i && !hasCategoriesStep();
        QFont font = label->font();
        font.setBold(i == current);
        label->setFont(font);
        label->setText(i < current && !skipped ? "✓ " + names[i] : names[i]);
        setTextColor(label, i == current ? colors.accent : i < current && !skipped ? colors.positive : colors.textMuted);
    }
}
