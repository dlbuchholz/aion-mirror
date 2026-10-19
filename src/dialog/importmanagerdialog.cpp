#include "importmanagerdialog.h"
#include "../../ui/ui_importmanagerdialog.h"
#include "account.h"
#include "category.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QHeaderView>
#include <QSignalBlocker>
#include "settings.h"
#include "csvparser.h"
#include "category.h"
#include "theme.h"
#include "widgets/categorytag.h"
#include <QCheckBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QGuiApplication>
#include <QScreen>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>

ImportManagerDialog::ImportManagerDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImportManagerDialog)
    , userId(userId)
    , csvDelimiter(',')
    , amountInterpretation(CsvImport::AmountInterpretation::SignBased)
    , autoDetectDelimiterOnNextParse(true)
{
    ui->setupUi(this);
    setupUI();

    // Define transaction fields
    transactionFields << CsvImport::FieldDate << CsvImport::FieldAmount << CsvImport::FieldPayee
                      << CsvImport::FieldDescription << CsvImport::FieldCategory << CsvImport::FieldType
                      << CsvImport::FieldBalance << "(Ignore)";

    // Set default decimal separator from Settings (will be overridden by auto-detection)
    Settings::DecimalSeparator appDecimalSeparator = Settings::instance().getDecimalSeparator();
    if (appDecimalSeparator == Settings::DecimalSeparator::Comma) {
        decimalSeparator = ',';
        ui->decimalSeparatorComboBox->setCurrentIndex(1);  // Comma
    } else {
        decimalSeparator = '.';
        ui->decimalSeparatorComboBox->setCurrentIndex(0);  // Point
    }

    populateAccountComboBox();
    setupColumnMappingTable();
    setupBalanceCheck();
    makeScrollable();

    // Connections
    connect(ui->selectFileButton, &QPushButton::clicked,
            this, &ImportManagerDialog::onSelectFileClicked);
    connect(ui->delimiterComboBox, &QComboBox::currentTextChanged,
            this, &ImportManagerDialog::onDelimiterChanged);
    connect(ui->decimalSeparatorComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportManagerDialog::onDecimalSeparatorChanged);
    connect(ui->amountInterpretationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportManagerDialog::onAmountInterpretationChanged);
    connect(ui->refreshPreviewButton, &QPushButton::clicked,
            this, &ImportManagerDialog::onPreviewRefreshClicked);
    connect(ui->dryRunButton, &QPushButton::clicked,
            this, &ImportManagerDialog::onDryRunClicked);
    connect(ui->importButton, &QPushButton::clicked,
            this, &ImportManagerDialog::onImportClicked);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &ImportManagerDialog::onCancelClicked);
}

ImportManagerDialog::~ImportManagerDialog()
{
    delete ui;
}

void ImportManagerDialog::setupUI()
{
    // Configure column mapping table
    ui->columnMappingTable->horizontalHeader()->setStretchLastSection(true);
    ui->columnMappingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->columnMappingTable->verticalHeader()->setVisible(false);

    // Configure preview table
    ui->previewTable->horizontalHeader()->setStretchLastSection(true);
    ui->previewTable->verticalHeader()->setVisible(true);
}

void ImportManagerDialog::populateAccountComboBox()
{
    AccountDAO accountDao;
    QList<Account> accounts = accountDao.getAllAccountsForUser(userId);

    ui->accountComboBox->clear();
    for (const Account& account : accounts) {
        ui->accountComboBox->addItem(account.getName(), account.getAccountId());
    }
}

void ImportManagerDialog::setupColumnMappingTable()
{
    ui->columnMappingTable->setRowCount(transactionFields.size());

    for (int i = 0; i < transactionFields.size(); ++i) {
        // Transaction field label (read-only)
        QTableWidgetItem *fieldItem = new QTableWidgetItem(transactionFields[i]);
        fieldItem->setFlags(fieldItem->flags() & ~Qt::ItemIsEditable);
        if (transactionFields[i] == CsvImport::FieldBalance) {
            fieldItem->setText("Balance (check only)");
            fieldItem->setToolTip("The bank's balance after each booking. Used to check the account's balance, never saved.");
        }
        ui->columnMappingTable->setItem(i, 0, fieldItem);

        // CSV column combo box
        QComboBox *comboBox = new QComboBox(this);
        comboBox->addItem("(Not Mapped)", -1);
        ui->columnMappingTable->setCellWidget(i, 1, comboBox);

        mappingComboBoxes[transactionFields[i]] = comboBox;

        connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ImportManagerDialog::onColumnMappingChanged);
    }
}

void ImportManagerDialog::onSelectFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select File to Import",
        QDir::homePath(),
        "CSV Files (*.csv);;All Files (*.*)"
        );

    if (filePath.isEmpty()) {
        return;
    }
    loadFile(filePath);
}

void ImportManagerDialog::loadFile(const QString &filePath)
{
    currentFilePath = filePath;
    autoDetectDelimiterOnNextParse = true;
    ui->filePathLineEdit->setText(filePath);

    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        parseCSV(filePath);
    } else {
        QMessageBox::warning(this, "Unsupported Format",
                             "Only CSV files are currently supported.");
    }
}

void ImportManagerDialog::parseCSV(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open file: " + file.errorString());
        return;
    }
    QTextStream in(&file);
    const QString text = in.readAll();
    file.close();

    if (autoDetectDelimiterOnNextParse) {
        autoDetectDelimiterOnNextParse = false;
        csvDelimiter = Settings::instance().isAutoDetectCsvDelimiter()
                           ? CsvImport::detectDelimiter(text)
                           : Settings::instance().getDefaultCsvDelimiter();

        const QSignalBlocker blocker(ui->delimiterComboBox);
        if (csvDelimiter == ',') {
            ui->delimiterComboBox->setCurrentIndex(0);  // Comma
        } else if (csvDelimiter == ';') {
            ui->delimiterComboBox->setCurrentIndex(1);  // Semicolon
        } else if (csvDelimiter == '\t') {
            ui->delimiterComboBox->setCurrentIndex(2);  // Tab
        } else if (csvDelimiter == '|') {
            ui->delimiterComboBox->setCurrentIndex(3);  // Pipe
        }
    }

    csvData.clear();
    csvHeaders.clear();

    QString parseError;
    const QList<QStringList> records = CsvParser::parse(text, csvDelimiter, &parseError);
    if (!parseError.isEmpty()) {
        QMessageBox::critical(this, "CSV Error", parseError);
        return;
    }
    if (!records.isEmpty()) {
        csvHeaders = records.first();
        for (qsizetype i = 1; i < records.size(); ++i) {
            csvData.append(records[i]);
        }
    }

    for (auto it = mappingComboBoxes.begin(); it != mappingComboBoxes.end(); ++it) {
        QComboBox *combo = it.value();
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem("(Not Mapped)", -1);
        for (int i = 0; i < csvHeaders.size(); ++i) combo->addItem(csvHeaders[i], i);
    }

    // Auto-map columns based on header names
    const CsvImport::Mapping mapping = CsvImport::autoMapColumns(csvHeaders);
    applyMappingToComboBoxes(mapping);

    // Detect the decimal separator from the amount column if enabled in settings
    const QChar appSeparator = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
    decimalSeparator = Settings::instance().isAutoDetectDecimalSeparator()
                           ? CsvImport::detectDecimalSeparator(csvData, mapping.value(CsvImport::FieldAmount, -1), appSeparator)
                           : appSeparator;
    {
        const QSignalBlocker blocker(ui->decimalSeparatorComboBox);
        ui->decimalSeparatorComboBox->setCurrentIndex(decimalSeparator == ',' ? 1 : 0);
    }

    updatePreviewTable();

    // Enable buttons
    ui->dryRunButton->setEnabled(true);
    ui->importButton->setEnabled(true);
}

void ImportManagerDialog::applyMappingToComboBoxes(const CsvImport::Mapping &mapping)
{
    for (auto it = mappingComboBoxes.begin(); it != mappingComboBoxes.end(); ++it) {
        QComboBox *combo = it.value();
        const QSignalBlocker blocker(combo);
        const int index = combo->findData(mapping.value(it.key(), -1));
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }
}

CsvImport::Mapping ImportManagerDialog::mappingFromComboBoxes() const
{
    CsvImport::Mapping mapping;
    for (auto it = mappingComboBoxes.begin(); it != mappingComboBoxes.end(); ++it) {
        const int columnIndex = it.value()->currentData().toInt();
        if (columnIndex >= 0) {
            mapping.insert(it.key(), columnIndex);
        }
    }
    return mapping;
}

void ImportManagerDialog::onDelimiterChanged(const QString &delimiter)
{
    autoDetectDelimiterOnNextParse = false;
    if (delimiter.contains("Comma")) {
        csvDelimiter = ',';
    } else if (delimiter.contains("Semicolon")) {
        csvDelimiter = ';';
    } else if (delimiter.contains("Tab")) {
        csvDelimiter = '\t';
    } else if (delimiter.contains("Pipe")) {
        csvDelimiter = '|';
    }

    // Re-parse file with new delimiter
    if (!currentFilePath.isEmpty()) {
        parseCSV(currentFilePath);
    }
}

void ImportManagerDialog::onDecimalSeparatorChanged(int index)
{
    // 0 = Point, 1 = Comma
    decimalSeparator = (index == 1) ? ',' : '.';
    updatePreviewTable();
}

void ImportManagerDialog::onAmountInterpretationChanged(int index)
{
    amountInterpretation = static_cast<CsvImport::AmountInterpretation>(index);
    updatePreviewTable();
}

void ImportManagerDialog::onColumnMappingChanged()
{
    updatePreviewTable();
}

void ImportManagerDialog::onPreviewRefreshClicked()
{
    updatePreviewTable();
}

void ImportManagerDialog::updatePreviewTable()
{
    refreshFileCategories();
    ui->previewTable->clear();
    ui->previewTable->setRowCount(0);
    ui->previewTable->setColumnCount(0);

    if (csvData.isEmpty()) {
        return;
    }

    const CsvImport::Mapping mapping = mappingFromComboBoxes();
    auto value = [&mapping](const QStringList &row, const QString &field) {
        const int column = mapping.value(field, -1);
        return (column >= 0 && column < row.size()) ? row[column] : QString();
    };

    const QStringList previewHeaders = {"Date", "Amount", "Type", "Payee", "Description", "Category"};
    ui->previewTable->setColumnCount(previewHeaders.size());
    ui->previewTable->setHorizontalHeaderLabels(previewHeaders);

    // Show first 10 rows
    const int previewRows = qMin(10, csvData.size());
    ui->previewTable->setRowCount(previewRows);

    for (int row = 0; row < previewRows; ++row) {
        const QStringList &csvRow = csvData[row];
        const QString amountText = value(csvRow, CsvImport::FieldAmount);
        const Cents amount = CsvImport::parseAmount(amountText, decimalSeparator);

        ui->previewTable->setItem(row, 0, new QTableWidgetItem(value(csvRow, CsvImport::FieldDate)));
        // Show parsed and original amount
        ui->previewTable->setItem(row, 1, new QTableWidgetItem(QString("%1 (%2)").arg(Money::toPlainString(amount), amountText)));
        ui->previewTable->setItem(row, 2, new QTableWidgetItem(
            CsvImport::transactionType(amount, value(csvRow, CsvImport::FieldType), amountInterpretation)));
        ui->previewTable->setItem(row, 3, new QTableWidgetItem(value(csvRow, CsvImport::FieldPayee)));
        ui->previewTable->setItem(row, 4, new QTableWidgetItem(value(csvRow, CsvImport::FieldDescription)));
        ui->previewTable->setItem(row, 5, new QTableWidgetItem(value(csvRow, CsvImport::FieldCategory)));
    }

    ui->previewTable->resizeColumnsToContents();
    scheduleBalanceCheck();
}

bool ImportManagerDialog::validateMapping()
{
    const CsvImport::Mapping mapping = mappingFromComboBoxes();

    // Check that required fields are mapped
    if (!mapping.contains(CsvImport::FieldDate)) {
        QMessageBox::warning(this, "Validation Error", "Date field must be mapped");
        return false;
    }
    if (!mapping.contains(CsvImport::FieldAmount)) {
        QMessageBox::warning(this, "Validation Error", "Amount field must be mapped");
        return false;
    }

    // Check that an account is selected
    if (ui->accountComboBox->currentIndex() < 0) {
        QMessageBox::warning(this, "Validation Error", "Please select an account");
        return false;
    }
    return true;
}

CsvImport::Result ImportManagerDialog::runImport(bool dryRun)
{
    CsvImport::Options options;
    options.accountId = ui->accountComboBox->currentData().toInt();
    options.decimalSeparator = decimalSeparator;
    options.interpretation = amountInterpretation;
    for (const Category &category : CategoryDAO().getAllCategories()) {
        options.categoryIds.insert(category.getName().simplified().toLower(), category.getCategoryId());
    }
    applyCategoryChoices(options);

    CsvImport::RunOptions runOptions;
    runOptions.allowDuplicates = ui->allowDuplicatesCheckBox->isChecked();
    runOptions.allowPartialImport = ui->allowPartialImportCheckBox->isChecked();
    runOptions.dryRun = dryRun;

    // Check the balance on the day the bank's balance column refers to, if it is mapped
    const CsvImport::StatedBalance stated = CsvImport::statedBalance(csvData, mappingFromComboBoxes(), decimalSeparator);
    if (stated.found) {
        runOptions.checkDate = stated.date;
    }
    Cents expected = 0;
    if (!dryRun && correctionWanted() && expectedBalance(&expected)) {
        runOptions.correct = true;
        runOptions.correctTo = expected;
        runOptions.correctionNote = QString("Import of %1").arg(QFileInfo(currentFilePath).fileName());
    }

    return CsvImport::run(csvData, mappingFromComboBoxes(), options, runOptions);
}

namespace {

// Scroll area content that never gets squeezed below its preferred height; the scroll
// bar takes over instead (otherwise wrapped labels and group boxes get cut off). The scroll
// area doesn't notice when sections grow later (the balance check after a dry run), so the
// content keeps its own minimum height up to date.
class FullHeightContent : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if ((event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) && layout()) {
            // Wrapped labels and tables need more height at narrower widths
            int height = layout()->sizeHint().height();
            if (layout()->hasHeightForWidth()) height = qMax(height, layout()->totalHeightForWidth(width()));
            if (minimumHeight() != height) {
                setMinimumHeight(height);
                if (this->height() < height) resize(width(), height);
            }
        }
        return handled;
    }
};

} // namespace

void ImportManagerDialog::makeScrollable()
{
    // Move the groups into a scroll area; the button row stays below it
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("importScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new FullHeightContent(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    while (ui->verticalLayout->count() > 0) {
        QLayoutItem *item = ui->verticalLayout->itemAt(0);
        if (item->layout() == ui->horizontalLayout_buttons) {
            break;
        }
        ui->verticalLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            contentLayout->addWidget(widget);
            delete item;
        } else {
            contentLayout->addItem(item);
        }
    }
    scroll->setWidget(content);
    ui->verticalLayout->insertWidget(0, scroll, 1);

    // Never taller than the screen
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        resize(qMin(width(), available.width() - 40), qMin(height(), available.height() - 80));
    }
}

void ImportManagerDialog::refreshFileCategories()
{
    if (!categoriesGroupBox) {
        return;
    }
    // Keep the user's choices while the table is rebuilt
    for (auto it = categoryChoices.cbegin(); it != categoryChoices.cend(); ++it) {
        chosenCategories.insert(it.key(), it.value()->currentData().toString());
    }
    categoryChoices.clear();
    delete categoriesScroll->takeWidget();
    categoriesTable = nullptr;

    CsvImport::Options options;
    options.decimalSeparator = decimalSeparator;
    options.interpretation = amountInterpretation;
    const QList<CsvImport::FileCategory> found = CsvImport::fileCategories(csvData, mappingFromComboBoxes(), options);
    categoriesGroupBox->setVisible(!found.isEmpty());
    if (found.isEmpty()) {
        return;
    }

    QHash<QString, Category> existing;
    QList<Category> choosable;
    for (const Category &category : CategoryDAO().getAllCategories()) {
        existing.insert(category.getName().simplified().toLower(), category);
        if (!category.isBuiltIn()) choosable.append(category);
    }

    categoriesTable = new QWidget();
    auto *grid = new QGridLayout(categoriesTable);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 2);
    int row = 0;
    for (const QString &caption : {QString("In the file"), QString("Rows"), QString("In Aion")}) {
        auto *header = new QLabel(caption, categoriesTable);
        header->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(header, 0, row++);
    }
    row = 1;
    for (const CsvImport::FileCategory &category : found) {
        const QString key = category.name.toLower();
        grid->addWidget(new QLabel(category.name, categoriesTable), row, 0);
        auto *count = new QLabel(QString::number(category.rows), categoriesTable);
        count->setForegroundRole(QPalette::PlaceholderText);
        grid->addWidget(count, row, 1);

        if (existing.contains(key)) {
            // Already known: just show it
            auto *match = new QWidget(categoriesTable);
            auto *layout = new QHBoxLayout(match);
            layout->setContentsMargins(0, 0, 0, 0);
            auto *tag = new CategoryTag(match);
            const Category &known = existing.find(key).value();
            tag->setCategory(known.getName(), known.getColor());
            layout->addWidget(tag);
            auto *note = new QLabel("matches", match);
            note->setForegroundRole(QPalette::PlaceholderText);
            layout->addWidget(note);
            layout->addStretch();
            grid->addWidget(match, row, 2);
        } else {
            auto *choice = new QComboBox(categoriesTable);
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
            connect(choice, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::scheduleBalanceCheck);
            connect(choice, &QComboBox::currentIndexChanged, this, &ImportManagerDialog::updateCategorySummary);
            categoryChoices.insert(key, choice);
            grid->addWidget(choice, row, 2);
        }
        ++row;
    }
    matchedCategories = int(found.size() - categoryChoices.size());

    // A long list scrolls inside the section instead of pushing the rest of the dialog away
    categoriesScroll->setWidget(categoriesTable);
    categoriesScroll->setFixedHeight(qMin(categoriesTable->sizeHint().height() + 2 * categoriesScroll->frameWidth(), 280));
    updateCategorySummary();
}

void ImportManagerDialog::updateCategorySummary()
{
    int create = 0;
    int none = 0;
    for (QComboBox *choice : std::as_const(categoryChoices)) {
        const QString data = choice->currentData().toString();
        if (data.startsWith("create:")) ++create;
        else if (data == "none") ++none;
    }
    const int mapped = int(categoryChoices.size()) - create - none;
    QStringList parts;
    if (matchedCategories > 0) parts << QString("%1 already in Aion").arg(matchedCategories);
    if (create > 0) parts << QString("%1 will be created").arg(create);
    if (mapped > 0) parts << QString("%1 mapped to other categories").arg(mapped);
    if (none > 0) parts << QString("%1 imported without a category").arg(none);
    categorySummaryLabel->setText(QString("%1 %2 in the file: %3.")
                                      .arg(matchedCategories + categoryChoices.size())
                                      .arg(matchedCategories + categoryChoices.size() == 1 ? "category" : "categories",
                                           parts.join(", ")));
    newCategoriesButton->setVisible(!categoryChoices.isEmpty());
}

void ImportManagerDialog::setAllNewCategories(bool create)
{
    for (QComboBox *choice : std::as_const(categoryChoices)) {
        const QSignalBlocker blocker(choice);
        choice->setCurrentIndex(create ? 0 : choice->findData("none"));   // Index 0: create with the likely type
    }
    updateCategorySummary();
    scheduleBalanceCheck();
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

void ImportManagerDialog::setupBalanceCheck()
{
    balanceGroupBox = new QGroupBox("Balance check", this);
    balanceGroupBox->setObjectName("balanceGroupBox");
    auto *layout = new QVBoxLayout(balanceGroupBox);

    balanceHintLabel = new QLabel(balanceGroupBox);
    balanceHintLabel->setObjectName("balanceHintLabel");
    balanceHintLabel->setWordWrap(true);
    balanceHintLabel->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(balanceHintLabel);

    balanceDetails = new QWidget(balanceGroupBox);
    auto *grid = new QGridLayout(balanceDetails);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(0, 1);
    auto addRow = [&](int row, QLabel **caption, QLabel **value, const QString &name) {
        *caption = new QLabel(balanceDetails);
        *value = new QLabel(balanceDetails);
        (*value)->setObjectName(name);
        (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(*caption, row, 0);
        grid->addWidget(*value, row, 1);
    };
    addRow(0, &balanceNowCaption, &balanceNowValue, "balanceNowValue");
    addRow(1, &balanceChangeCaption, &balanceChangeValue, "balanceChangeValue");
    addRow(2, &balanceAfterCaption, &balanceAfterValue, "balanceAfterValue");
    for (QLabel *label : {balanceAfterCaption, balanceAfterValue}) {
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
    }

    expectedCaption = new QLabel(balanceDetails);
    expectedEdit = new QLineEdit(balanceDetails);
    expectedEdit->setObjectName("expectedBalanceEdit");
    expectedEdit->setPlaceholderText("Optional");
    expectedEdit->setAlignment(Qt::AlignRight);
    expectedEdit->setMaximumWidth(160);
    grid->addWidget(expectedCaption, 3, 0);
    grid->addWidget(expectedEdit, 3, 1);
    expectedSourceLabel = new QLabel(balanceDetails);
    expectedSourceLabel->setForegroundRole(QPalette::PlaceholderText);
    grid->addWidget(expectedSourceLabel, 4, 0, 1, 2);
    connect(expectedEdit, &QLineEdit::textEdited, this, [this]() {
        expectedEditedByUser = true;
        expectedSourceLabel->clear();
        updateBalanceStatus();
    });

    balanceStatusLabel = new QLabel(balanceDetails);
    balanceStatusLabel->setObjectName("balanceStatusLabel");
    balanceStatusLabel->setWordWrap(true);
    grid->addWidget(balanceStatusLabel, 5, 0, 1, 2);
    correctionCheckBox = new QCheckBox(balanceDetails);
    correctionCheckBox->setObjectName("correctionCheckBox");
    correctionCheckBox->setChecked(true);
    // Hidden parts keep their space so the section never changes height
    for (QWidget *widget : {static_cast<QWidget *>(correctionCheckBox), static_cast<QWidget *>(expectedSourceLabel)}) {
        QSizePolicy policy = widget->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        widget->setSizePolicy(policy);
    }
    grid->addWidget(correctionCheckBox, 6, 0, 1, 2);
    layout->addWidget(balanceDetails);

    // Categories in the file, then the balance check, between the preview and the buttons
    categoriesGroupBox = new QGroupBox("Categories in this file", this);
    categoriesGroupBox->setObjectName("categoriesGroupBox");
    auto *categoriesLayout = new QVBoxLayout(categoriesGroupBox);
    auto *summaryRow = new QHBoxLayout();
    categorySummaryLabel = new QLabel(categoriesGroupBox);
    categorySummaryLabel->setObjectName("categorySummaryLabel");
    categorySummaryLabel->setWordWrap(true);
    summaryRow->addWidget(categorySummaryLabel, 1);
    newCategoriesButton = new QToolButton(categoriesGroupBox);
    newCategoriesButton->setObjectName("newCategoriesButton");
    newCategoriesButton->setText("All new categories");
    newCategoriesButton->setPopupMode(QToolButton::InstantPopup);
    auto *newCategoriesMenu = new QMenu(newCategoriesButton);
    newCategoriesMenu->addAction("Create them", this, [this]() { setAllNewCategories(true); });
    newCategoriesMenu->addAction("Import without a category", this, [this]() { setAllNewCategories(false); });
    newCategoriesButton->setMenu(newCategoriesMenu);
    summaryRow->addWidget(newCategoriesButton);
    categoriesLayout->addLayout(summaryRow);
    categoriesScroll = new QScrollArea(categoriesGroupBox);
    categoriesScroll->setObjectName("categoriesScroll");
    categoriesScroll->setWidgetResizable(true);
    categoriesScroll->setFrameShape(QFrame::StyledPanel);
    categoriesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    categoriesLayout->addWidget(categoriesScroll);
    categoriesGroupBox->hide();
    ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(ui->previewGroupBox) + 1, categoriesGroupBox);
    ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(categoriesGroupBox) + 1, balanceGroupBox);

    balanceTimer = new QTimer(this);
    balanceTimer->setSingleShot(true);
    balanceTimer->setInterval(250);
    connect(balanceTimer, &QTimer::timeout, this, &ImportManagerDialog::refreshBalanceCheck);
    connect(ui->accountComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        expectedEditedByUser = false;                // Another account has another balance
        scheduleBalanceCheck();
    });
    connect(ui->allowDuplicatesCheckBox, &QCheckBox::toggled, this, &ImportManagerDialog::scheduleBalanceCheck);
    connect(ui->allowPartialImportCheckBox, &QCheckBox::toggled, this, &ImportManagerDialog::scheduleBalanceCheck);
    refreshBalanceCheck();
}

void ImportManagerDialog::scheduleBalanceCheck()
{
    if (balanceTimer) {
        balanceTimer->start();
    }
}

void ImportManagerDialog::refreshBalanceCheck()
{
    const CsvImport::Mapping mapping = mappingFromComboBoxes();
    balanceReady = !csvData.isEmpty() && mapping.contains(CsvImport::FieldDate) && mapping.contains(CsvImport::FieldAmount)
                   && ui->accountComboBox->currentIndex() >= 0;
    if (balanceReady) {
        balanceResult = runImport(true);
        balanceReady = balanceResult.checkDate.isValid();
    }
    // The rows stay in place (with dashes until there is something to show), so the section
    // keeps its height and the scrolling layout around it never has to catch up
    balanceDetails->setEnabled(balanceReady);
    balanceHintLabel->setText(balanceReady
        ? QString("Compare the account's balance after the import with the one your bank shows. Nothing is saved until you import.")
        : QString("Choose a file and map its date and amount columns to check the account's balance after the import."));
    if (!balanceReady) {
        for (QLabel *value : {balanceNowValue, balanceChangeValue, balanceAfterValue}) value->setText("–");
        balanceNowCaption->setText("Balance now");
        balanceChangeCaption->setText("New transactions");
        balanceAfterCaption->setText("Balance after import");
        expectedCaption->setText("Your bank's balance");
        balanceStatusLabel->clear();
        correctionCheckBox->hide();
        return;
    }

    const Settings &settings = Settings::instance();
    const QLocale english(QLocale::English);
    const QString day = english.toString(balanceResult.checkDate, "d MMMM yyyy");
    const Cents change = balanceResult.balanceAfterCents - balanceResult.balanceBeforeCents;
    balanceNowCaption->setText(QString("Balance on %1 now").arg(day));
    balanceNowValue->setText(settings.formatCurrency(balanceResult.balanceBeforeCents));
    QString changeText = QString("%1 new %2").arg(balanceResult.imported).arg(balanceResult.imported == 1 ? "transaction" : "transactions");
    if (balanceResult.duplicates > 0) {
        changeText += QString(" · %1 %2 skipped").arg(balanceResult.duplicates).arg(balanceResult.duplicates == 1 ? "duplicate" : "duplicates");
    }
    balanceChangeCaption->setText(changeText);
    balanceChangeValue->setText(settings.formatCurrency(change, true));
    balanceAfterCaption->setText(QString("Balance on %1 after import").arg(day));
    balanceAfterValue->setText(settings.formatCurrency(balanceResult.balanceAfterCents));
    expectedCaption->setText(QString("Your bank's balance on %1").arg(day));

    // Fill in the bank's balance from the file unless the user typed one
    if (!expectedEditedByUser) {
        const CsvImport::StatedBalance stated = CsvImport::statedBalance(csvData, mapping, decimalSeparator);
        expectedEdit->setText(stated.found ? settings.formatCurrency(stated.cents) : QString());
        expectedSourceLabel->setText(stated.found ? QString("From the file's \"%1\" column").arg(csvHeaders.value(mapping.value(CsvImport::FieldBalance)))
                                                  : QString(" "));   // Keeps its line
    }
    updateBalanceStatus();
}

bool ImportManagerDialog::expectedBalance(Cents *cents) const
{
    if (!expectedEdit || expectedEdit->text().trimmed().isEmpty()) {
        return false;
    }
    const QChar separator = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
    bool ok = false;
    *cents = CsvImport::parseAmount(expectedEdit->text(), separator, &ok);
    return ok;
}

bool ImportManagerDialog::correctionWanted() const
{
    return balanceReady && correctionCheckBox && correctionCheckBox->isVisible() && correctionCheckBox->isChecked();
}

void ImportManagerDialog::updateBalanceStatus()
{
    if (!balanceReady) {
        return;
    }
    const Settings &settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    QPalette statusPalette = balanceStatusLabel->palette();

    Cents expected = 0;
    if (expectedEdit->text().trimmed().isEmpty()) {
        balanceStatusLabel->setText("Enter the balance your bank shows to compare.");
        statusPalette.setColor(QPalette::WindowText, colors.textSecondary);
        correctionCheckBox->hide();
    } else if (!expectedBalance(&expected)) {
        balanceStatusLabel->setText("That isn't an amount Aion can read.");
        statusPalette.setColor(QPalette::WindowText, colors.danger);
        correctionCheckBox->hide();
    } else if (expected == balanceResult.balanceAfterCents) {
        balanceStatusLabel->setText("Matches your bank after the import.");
        statusPalette.setColor(QPalette::WindowText, colors.positive);
        correctionCheckBox->hide();
    } else {
        const Cents difference = expected - balanceResult.balanceAfterCents;
        balanceStatusLabel->setText(QString("%1 %2 than your bank after the import. Missing or extra bookings, for example "
                                            "from a gap between exports, can cause this.")
                                        .arg(settings.formatCurrency(qAbs(difference)), difference > 0 ? "less" : "more"));
        statusPalette.setColor(QPalette::WindowText, colors.danger);
        correctionCheckBox->setText(QString("Add a balance correction of %1 on %2")
                                        .arg(settings.formatCurrency(difference, true),
                                             QLocale(QLocale::English).toString(balanceResult.checkDate, "d MMMM")));
        correctionCheckBox->show();
    }
    balanceStatusLabel->setPalette(statusPalette);
}

void ImportManagerDialog::onDryRunClicked()
{
    if (!validateMapping()) {
        return;
    }
    const CsvImport::Result result = runImport(true);

    // Build detailed report
    QString message = QString("Dry Run Results:\n\n");
    message += QString("✅ Valid transactions:    %1\n").arg(result.imported);
    message += QString("⚠️  Duplicate transactions: %1\n").arg(result.duplicates);
    message += QString("❌ Failed validations:  %1\n").arg(result.failed);

    if (result.imported > 0) {
        message += QString("\n%1 transactions would be imported. ").arg(result.imported);
    }
    if (result.duplicates > 0) {
        message += QString("\n%1 transactions would be skipped (duplicates).").arg(result.duplicates);
    }

    if (result.failed > 0) {
        if (ui->allowPartialImportCheckBox->isChecked()) {
            message += QString("\n\n⚠️  Partial import is enabled:  %1 valid transactions would still be imported despite %2 errors.")
                           .arg(result.imported)
                           .arg(result.failed);
        } else {
            message += QString("\n\n❌ Partial import is disabled:  Import would be aborted due to errors.");
        }

        message += "\n\nErrors: ";
        const int errorCount = qMin(10, result.errors.size());
        for (int i = 0; i < errorCount; ++i) {
            message += "\n• " + result.errors[i];
        }
        if (result.errors.size() > 10) {
            message += QString("\n... and %1 more errors").arg(result.errors.size() - 10);
        }
    }

    message += "\n\nNo data has been written to the database.";

    if (result.failed > 0) {
        QMessageBox::warning(this, "Dry Run Complete", message);
    } else {
        QMessageBox::information(this, "Dry Run Complete", message);
    }
}

void ImportManagerDialog::onImportClicked()
{
    if (!validateMapping()) {
        return;
    }
    const CsvImport::Result result = runImport(false);

    // Show detailed results
    QString message = QString("Import Results:\n\n");
    message += QString("✅ Successfully imported: %1 transactions\n").arg(result.imported);

    if (result.duplicates > 0) {
        message += QString("⚠️  Skipped duplicates: %1 transactions\n").arg(result.duplicates);
    }

    if (result.correctionCents != 0) {
        message += QString("Balance correction: %1 on %2\n")
                       .arg(Settings::instance().formatCurrency(result.correctionCents, true),
                            QLocale(QLocale::English).toString(result.checkDate, "d MMMM yyyy"));
    }
    if (result.categoriesCreated > 0) {
        message += QString("New categories: %1\n").arg(result.categoriesCreated);
    }

    if (result.failed > 0) {
        message += QString("❌ Failed:  %1 transactions\n").arg(result.failed);

        if (!result.errors.isEmpty()) {
            message += "\n\nErrors:";
            const int errorCount = qMin(10, result.errors.size());
            for (int i = 0; i < errorCount; ++i) {
                message += "\n• " + result.errors[i];
            }
            if (result.errors.size() > 10) {
                message += QString("\n... and %1 more errors (check console)").arg(result.errors.size() - 10);
            }
        }

        QMessageBox::warning(this, "Import Complete with Errors", message);
        for (const QString &error : result.errors) {
            qWarning() << error;
        }
    } else {
        QMessageBox::information(this, "Import Complete", message);
    }

    if (result.imported > 0 || result.correctionCents != 0) {
        emit transactionsImported(result.imported);
        accept();
    } else if (result.duplicates > 0 && result.failed == 0) {
        QMessageBox::information(this, "Import Complete",
                                 "All transactions were duplicates and have been skipped.");
        reject();
    } else if (result.failed > 0 && result.imported == 0) {
        QMessageBox::critical(this, "Import Failed",
                              "No transactions were imported due to errors.");
        reject();
    }
}

void ImportManagerDialog::onCancelClicked()
{
    reject();
}
