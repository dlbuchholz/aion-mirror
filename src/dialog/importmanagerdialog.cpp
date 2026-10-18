#include "importmanagerdialog.h"
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
                      << "(Ignore)";

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
        options.categoryIds.insert(category.getName().trimmed().toLower(), category.getCategoryId());
    }

    CsvImport::RunOptions runOptions;
    runOptions.allowDuplicates = ui->allowDuplicatesCheckBox->isChecked();
    runOptions.allowPartialImport = ui->allowPartialImportCheckBox->isChecked();
    runOptions.dryRun = dryRun;

    return CsvImport::run(csvData, mappingFromComboBoxes(), options, runOptions);
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

    if (result.imported > 0) {
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
