#include "importmanagerdialog.h"
#include "account.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QHeaderView>
#include <QDate>
#include <QRegularExpression>
#include <QSignalBlocker>
#include "settings.h"
#include "csvparser.h"
#include <db/databasemanager.h>

ImportManagerDialog::ImportManagerDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ImportManagerDialog)
    , userId(userId)
    , currentFormat(FileFormat::CSV)
    , csvDelimiter(',')
    , amountInterpretation(AmountInterpretation::SignBased)
    , autoDetectDelimiterOnNextParse(true)
{
    ui->setupUi(this);
    setupUI();

    // Define transaction fields
    transactionFields << "Date" << "Amount" << "Payee" << "Description"
                      << "Category" << "Type" << "(Ignore)";

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

    qDebug() << "Populated account combo box with" << accounts.size() << "accounts";
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

    // Determine format based on extension
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        currentFormat = FileFormat::CSV;
        loadFile(filePath);
    } else {
        QMessageBox::warning(this, "Unsupported Format",
                             "Only CSV files are currently supported.");
    }
}

void ImportManagerDialog::loadFile(const QString &filePath)
{
    if (currentFormat == FileFormat::CSV) {
        parseCSV(filePath);
    } else {
        QMessageBox::warning(this, "Error", "Unsupported file format");
    }
}

void ImportManagerDialog::parseCSV(const QString &filePath)
{
    if (autoDetectDelimiterOnNextParse) {
        autoDetectDelimiterOnNextParse = false;
        if (Settings::instance().isAutoDetectCsvDelimiter()) {
            csvDelimiter = detectCsvDelimiter(filePath);
            qDebug() << "Auto-detection enabled:   detected delimiter:" << csvDelimiter;
        } else {
            csvDelimiter = Settings::instance().getDefaultCsvDelimiter();
            qDebug() << "Auto-detection disabled:  using default delimiter:" << csvDelimiter;
        }

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

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open file: " + file.errorString());
        return;
    }

    csvData.clear();
    csvHeaders.clear();

    QTextStream in(&file);
    QString parseError;
    const QList<QStringList> records = CsvParser::parse(in.readAll(), csvDelimiter, &parseError);
    file.close();

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
        combo->clear();
        combo->addItem("(Not Mapped)", -1);
        for (int i = 0; i < csvHeaders.size(); ++i) combo->addItem(csvHeaders[i], i);
    }

    qDebug() << "Parsed CSV:" << csvData.size() << "rows," << csvHeaders.size() << "columns";

    // Auto-map columns based on header names
    autoMapColumns();

    // AUTO-DETECT DECIMAL SEPARATOR if enabled in settings
    int amountColumnIndex = -1;
    QComboBox *amountCombo = mappingComboBoxes. value("Amount");
    if (amountCombo) {
        amountColumnIndex = amountCombo->currentData().toInt();
    }

    if (Settings::instance().isAutoDetectDecimalSeparator()) {
        decimalSeparator = detectDecimalSeparator(csvData, amountColumnIndex);
        qDebug() << "Auto-detection enabled:  detected decimal separator:" << decimalSeparator;
    } else {
        // Use the app's currency decimal separator setting
        Settings::DecimalSeparator appDecimalSeparator = Settings::instance().getDecimalSeparator();
        decimalSeparator = (appDecimalSeparator == Settings::DecimalSeparator:: Comma) ? ',' : '.';
        qDebug() << "Auto-detection disabled: using default decimal separator:" << decimalSeparator;
    }

    // Update UI to reflect decimal separator
    if (decimalSeparator == ',') {
        ui->decimalSeparatorComboBox->setCurrentIndex(1);  // Comma
    } else {
        ui->decimalSeparatorComboBox->setCurrentIndex(0);  // Point
    }

    // Update preview
    updatePreviewTable();

    // Enable buttons
    ui->dryRunButton->setEnabled(true);
    ui->importButton->setEnabled(true);
}

void ImportManagerDialog::autoMapColumns()
{
    // Try to automatically map columns based on common header names
    QMap<QString, QStringList> commonNames;

    // Date field
    commonNames["Date"] = {
        "date",
        "transaction date",
        "datum",
        "trans.  date",
        "buchungstag",           // Booking date
        "valutadatum",           // Value date (alternative)
        "wertstellung"           // Value date (alternative)
    };

    // Amount field
    commonNames["Amount"] = {
        "amount",
        "value",
        "betrag",                // Amount
        "sum",
        "wert",
        "umsatz"                 // Transaction amount (alternative)
    };

    // Payee field
    commonNames["Payee"] = {
        "payee",
        "merchant",
        "empfänger",             // Recipient
        "recipient",
        "name zahlungsbeteiligter",  // Name of payment participant
        "auftraggeber",          // Payer/Principal
        "zahlungsempfänger",     // Payment recipient
        "begünstigter",          // Beneficiary
        "name"                   // Generic name
    };

    // Description field
    commonNames["Description"] = {
        "description",
        "memo",
        "note",
        "beschreibung",
        "details",
        "verwendungszweck",      // Purpose/Reference
        "verwendung",            // Usage
        "zweck",                 // Purpose
        "referenz"               // Reference
    };

    // Category field
    commonNames["Category"] = {
        "category",
        "kategorie",             // Category
        "type",
        "art",
        "transaktionsart"        // Transaction type
    };

    // Type field (income/expense indicator)
    commonNames["Type"] = {
        "type",
        "transaction type",
        "typ",
        "art",
        "soll/haben",            // Debit/Credit
        "buchungsart"            // Booking type
    };


    for (auto it = commonNames.begin(); it != commonNames.end(); ++it) {
        QString field = it.key();
        QStringList possibleNames = it.value();

        QComboBox *combo = mappingComboBoxes[field];

        for (int i = 0; i < csvHeaders.size(); ++i) {
            QString header = csvHeaders[i]. trimmed().toLower();

            if (possibleNames.contains(header)) {
                combo->setCurrentIndex(combo->findData(i));
                qDebug() << "Auto-mapped" << field << "to column" << i << "(" << csvHeaders[i] << ")";
                break;
            }
        }
    }
}

QChar ImportManagerDialog:: detectCsvDelimiter(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for delimiter detection";
        return ',';  // Default fallback
    }

    QTextStream in(&file);
    const QString sample = in.readAll();
    file.close();

    if (sample.isEmpty()) {
        return ',';
    }

    QMap<QChar, int> delimiterCounts;
    QList<QChar> possibleDelimiters = {',', ';', '\t', '|'};

    for (QChar delimiter : possibleDelimiters) {
        QString error;
        const QList<QStringList> records = CsvParser::parse(sample, delimiter, &error);
        if (!error.isEmpty() || records.isEmpty() || records.first().size() <= 1) continue;
        const int columnCount = records.first().size();
        bool consistent = true;
        for (const QStringList& record : records) {
            if (record.size() != columnCount) {
                consistent = false;
                break;
            }
        }
        if (consistent) delimiterCounts[delimiter] = columnCount - 1;
    }

    // Find delimiter with highest consistent count
    QChar bestDelimiter = ',';
    int maxCount = 0;

    for (auto it = delimiterCounts.begin(); it != delimiterCounts. end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            bestDelimiter = it.key();
        }
    }

    QString delimiterName;
    if (bestDelimiter == ',') delimiterName = "Comma";
    else if (bestDelimiter == ';') delimiterName = "Semicolon";
    else if (bestDelimiter == '\t') delimiterName = "Tab";
    else if (bestDelimiter == '|') delimiterName = "Pipe";

    qDebug() << "Auto-detected CSV delimiter:" << delimiterName << "(" << bestDelimiter << ")";

    return bestDelimiter;
}

QChar ImportManagerDialog::detectDecimalSeparator(const QList<QStringList>& sampleData, int amountColumnIndex)
{
    // Count occurrences of comma and point in numeric-looking values
    int commaCount = 0;
    int pointCount = 0;

    QRegularExpression numericPattern("\\d+[.,]\\d+");  // Match numbers with comma or point

    // If we know the amount column, focus on that
    QStringList dataToCheck;
    if (amountColumnIndex >= 0 && ! sampleData.isEmpty()) {
        for (const QStringList& row : sampleData) {
            if (amountColumnIndex < row.size()) {
                dataToCheck.append(row[amountColumnIndex]);
            }
        }
    } else {
        for (const QStringList& row : sampleData) dataToCheck.append(row);
    }

    for (const QString &value : dataToCheck) {
        QRegularExpressionMatch match = numericPattern.match(value);
        if (match.hasMatch()) {
            QString matched = match.captured(0);

            // Check the last separator (decimal separator is usually the last one)
            int lastComma = matched.lastIndexOf(',');
            int lastPoint = matched.lastIndexOf('.');

            if (lastComma > lastPoint) {
                commaCount++;
            } else if (lastPoint > lastComma) {
                pointCount++;
            }
        }
    }

    // Determine which is more common
    QChar detectedSeparator;
    if (commaCount > pointCount) {
        detectedSeparator = ',';
        qDebug() << "Auto-detected decimal separator:  Comma (found" << commaCount << "instances)";
    } else if (pointCount > commaCount) {
        detectedSeparator = '.';
        qDebug() << "Auto-detected decimal separator: Point (found" << pointCount << "instances)";
    } else {
        // No clear winner, use app settings default
        Settings:: DecimalSeparator appDecimalSeparator = Settings::instance().getDecimalSeparator();
        detectedSeparator = (appDecimalSeparator == Settings::DecimalSeparator:: Comma) ? ',' : '.';
        qDebug() << "Could not auto-detect decimal separator, using app default:" << detectedSeparator;
    }

    return detectedSeparator;
}

void ImportManagerDialog::onDelimiterChanged(const QString &delimiter)
{
    autoDetectDelimiterOnNextParse = false;
    if (delimiter. contains("Comma")) {
        csvDelimiter = ',';
    } else if (delimiter.contains("Semicolon")) {
        csvDelimiter = ';';
    } else if (delimiter.contains("Tab")) {
        csvDelimiter = '\t';
    } else if (delimiter.contains("Pipe")) {
        csvDelimiter = '|';
    }

    // Re-parse file with new delimiter
    if (! currentFilePath.isEmpty()) {
        parseCSV(currentFilePath);
    }
}

void ImportManagerDialog::onDecimalSeparatorChanged(int index)
{
    // 0 = Point, 1 = Comma
    decimalSeparator = (index == 1) ? ',' : '.';

    qDebug() << "Decimal separator changed to:" << decimalSeparator;

    // Refresh preview with new decimal separator
    updatePreviewTable();
}

void ImportManagerDialog::onAmountInterpretationChanged(int index)
{
    amountInterpretation = static_cast<AmountInterpretation>(index);
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

void ImportManagerDialog::onDryRunClicked()
{
    qDebug() << "=== DRY RUN MODE ===";

    ImportResult result = processTransactions(false);  // false = don't write to DB

    // Build detailed report
    QString message = QString("Dry Run Results:\n\n");
    message += QString("✅ Valid transactions:    %1\n").arg(result.successCount);
    message += QString("⚠️  Duplicate transactions: %1\n").arg(result.duplicateCount);
    message += QString("❌ Failed validations:  %1\n").arg(result.failCount);

    if (result.successCount > 0) {
        message += QString("\n%1 transactions would be imported. ").arg(result.successCount);
    }

    if (result.duplicateCount > 0) {
        message += QString("\n%1 transactions would be skipped (duplicates).").arg(result.duplicateCount);
    }

    if (result.failCount > 0) {
        bool partialEnabled = ui->allowPartialImportCheckBox->isChecked();
        if (partialEnabled) {
            message += QString("\n\n⚠️  Partial import is enabled:  %1 valid transactions would still be imported despite %2 errors.")
                           .arg(result.successCount)
                           .arg(result.failCount);
        } else {
            message += QString("\n\n❌ Partial import is disabled:  Import would be aborted due to errors.");
        }

        message += "\n\nErrors: ";
        int errorCount = qMin(10, result.errors.size());
        for (int i = 0; i < errorCount; ++i) {
            message += "\n• " + result.errors[i];
        }
        if (result.errors.size() > 10) {
            message += QString("\n... and %1 more errors").arg(result.errors.size() - 10);
        }
    }

    message += "\n\nNo data has been written to the database.";

    if (result.failCount > 0) {
        QMessageBox::warning(this, "Dry Run Complete", message);
    } else {
        QMessageBox::information(this, "Dry Run Complete", message);
    }

    // Log all errors to console
    if (! result.errors.isEmpty()) {
        qWarning() << "=== Dry Run Errors ===";
        for (const QString &error : result.errors) {
            qWarning() << error;
        }
    }
}

void ImportManagerDialog::updatePreviewTable()
{
    ui->previewTable->clear();
    ui->previewTable->setRowCount(0);
    ui->previewTable->setColumnCount(0);

    if (csvData.isEmpty()) {
        return;
    }

    // Build column mapping
    columnMapping.clear();
    for (auto it = mappingComboBoxes.begin(); it != mappingComboBoxes.end(); ++it) {
        QString field = it.key();
        QComboBox *combo = it.value();
        int columnIndex = combo->currentData().toInt();

        if (columnIndex >= 0) {
            columnMapping[field] = columnIndex;
        }
    }

    // Set up preview table columns
    QStringList previewHeaders;
    previewHeaders << "Date" << "Amount" << "Type" << "Payee" << "Description" << "Category";

    ui->previewTable->setColumnCount(previewHeaders.size());
    ui->previewTable->setHorizontalHeaderLabels(previewHeaders);

    // Show first 10 rows
    int previewRows = qMin(10, csvData.size());
    ui->previewTable->setRowCount(previewRows);

    for (int row = 0; row < previewRows; ++row) {
        QStringList csvRow = csvData[row];

        // Date
        int dateCol = columnMapping.value("Date", -1);
        QString dateStr = (dateCol >= 0 && dateCol < csvRow.size()) ? csvRow[dateCol] : "";
        ui->previewTable->setItem(row, 0, new QTableWidgetItem(dateStr));

        // Amount - parseAmount() instead of toDouble()
        int amountCol = columnMapping.value("Amount", -1);
        double amount = 0.0;
        QString amountStr = "";
        if (amountCol >= 0 && amountCol < csvRow. size()) {
            amountStr = csvRow[amountCol];
            amount = parseAmount(amountStr);
        }

        // Show original and parsed amount
        QString amountDisplay = QString("%1 (%2)").arg(amount, 0, 'f', 2).arg(amountStr);
        ui->previewTable->setItem(row, 1, new QTableWidgetItem(amountDisplay));

        // Type (determined by amount interpretation)
        int typeCol = columnMapping.value("Type", -1);
        QString typeStr = (typeCol >= 0 && typeCol < csvRow.size()) ? csvRow[typeCol] : "";
        QString transactionType = determineTransactionType(amount, typeStr);
        ui->previewTable->setItem(row, 2, new QTableWidgetItem(transactionType));

        // Payee
        int payeeCol = columnMapping.value("Payee", -1);
        QString payeeStr = (payeeCol >= 0 && payeeCol < csvRow.size()) ? csvRow[payeeCol] : "";
        ui->previewTable->setItem(row, 3, new QTableWidgetItem(payeeStr));

        // Description
        int descCol = columnMapping.value("Description", -1);
        QString descStr = (descCol >= 0 && descCol < csvRow. size()) ? csvRow[descCol] : "";
        ui->previewTable->setItem(row, 4, new QTableWidgetItem(descStr));

        // Category
        int catCol = columnMapping.value("Category", -1);
        QString catStr = (catCol >= 0 && catCol < csvRow.size()) ? csvRow[catCol] : "";
        ui->previewTable->setItem(row, 5, new QTableWidgetItem(catStr));
    }

    ui->previewTable->resizeColumnsToContents();
}

QString ImportManagerDialog::determineTransactionType(double amount, const QString &typeColumn)
{
    switch (amountInterpretation) {
    case AmountInterpretation::SignBased:
        return (amount < 0) ? "expense" : "income";

    case AmountInterpretation::SignBasedInverted:
        return (amount < 0) ? "income" : "expense";

    case AmountInterpretation::AllIncome:
        return "income";

    case AmountInterpretation::AllExpense:
        return "expense";

    case AmountInterpretation:: ColumnBased:
        // Use the Type column if mapped
        if (! typeColumn.isEmpty()) {
            QString lower = typeColumn.toLower();
            if (lower. contains("income") || lower.contains("credit")) {
                return "income";
            } else if (lower.contains("expense") || lower.contains("debit")) {
                return "expense";
            }
        }
        // Fallback to sign-based
        return (amount < 0) ? "expense" : "income";

    default:
        return "expense";
    }
}

QDate ImportManagerDialog::parseDate(const QString &dateStr)
{
    if (dateStr.trimmed().isEmpty()) {
        qWarning() << "Empty date string";
        return QDate();
    }

    QString cleaned = dateStr.trimmed();
    QDate date;

    // List of common date formats to try
    QStringList formats = {
        "yyyy-MM-dd",      // ISO format:  2024-01-15
        "dd/MM/yyyy",      // European:  15/01/2024
        "MM/dd/yyyy",      // US: 01/15/2024
        "dd.MM.yyyy",      // German: 15.01.2024
        "yyyy/MM/dd",      // Alternative ISO:  2024/01/15
        "d/M/yyyy",        // Short: 5/1/2024
        "d. M.yyyy",        // Short German: 5.1.2024
        "dd-MM-yyyy",      // Dashed European: 15-01-2024
        "MM-dd-yyyy",      // Dashed US: 01-15-2024
        "yyyyMMdd",        // Compact: 20240115
        "dd MMM yyyy",     // 15 Jan 2024
        "MMM dd, yyyy",    // Jan 15, 2024
    };

    // Try each format
    for (const QString &format : formats) {
        date = QDate::fromString(cleaned, format);
        if (date.isValid()) {
            qDebug() << "Successfully parsed date:" << dateStr << "with format:" << format << "-> " << date;
            return date;
        }
    }

    // Try to parse with QLocale (handles locale-specific formats)
    date = QLocale().toDate(cleaned, QLocale::ShortFormat);
    if (date.isValid()) {
        qDebug() << "Successfully parsed date with locale:" << dateStr << "->" << date;
        return date;
    }

    qWarning() << "Failed to parse date:" << dateStr << "- tried all formats";
    return QDate();  // Invalid date
}

double ImportManagerDialog::parseAmount(const QString &amountStr)
{
    if (amountStr.trimmed().isEmpty()) {
        return 0.0;
    }

    QString cleaned = amountStr.trimmed();

    // Remove any whitespace
    cleaned.remove(QRegularExpression("\\s+"));

    // Remove currency symbols (€, $, £, ¥, etc.) and other non-numeric chars except .,+-
    QRegularExpression nonNumericRegex("[^0-9.,\\-+]");
    cleaned.remove(nonNumericRegex);

    // Handle parentheses notation for negative (e.g., "(123. 45)" = -123.45)
    // This is already removed by the regex above, but we track it
    bool isNegative = amountStr.contains('(') || cleaned.startsWith('-');
    cleaned. remove('(').remove(')').remove('+');

    // Now determine which separator is decimal and which is thousands
    if (decimalSeparator == ',') {
        // European format: 1.234,56
        // Dots are thousands, comma is decimal
        cleaned.remove('.');
        cleaned.replace(',', '.');
    } else {
        // US format: 1,234.56
        // Commas are thousands, dot is decimal
        cleaned.remove(',');
        // Dot already correct
    }

    // Handle negative sign
    if (isNegative && ! cleaned.startsWith('-')) {
        cleaned.prepend('-');
    }

    bool ok;
    double value = cleaned.toDouble(&ok);

    if (!ok) {
        qWarning() << "Failed to parse amount:" << amountStr << "-> cleaned:" << cleaned;
        return 0.0;
    }

    return value;
}

ImportManagerDialog::ImportResult ImportManagerDialog::processTransactions(bool writeToDatabase)
{
    ImportResult result;
    result.successCount = 0;
    result. duplicateCount = 0;
    result.failCount = 0;

    if (!validateMapping()) {
        return result;
    }

    int accountId = ui->accountComboBox->currentData().toInt();
    TransactionDAO transactionDao;

    qDebug() << "=== Pre-processing transactions ===" << (writeToDatabase ? "(WRITE MODE)" : "(DRY RUN)");

    // Parse all transactions (same parsing logic as before)
    for (int i = 0; i < csvData. size(); ++i) {
        const QStringList &csvRow = csvData[i];

        // Parse date
        int dateCol = columnMapping.value("Date", -1);
        if (dateCol < 0 || dateCol >= csvRow.size()) {
            result.errors << QString("Row %1: Date column not mapped or out of range").arg(i + 2);
            result.failCount++;
            continue;
        }

        QDate date = parseDate(csvRow[dateCol]);
        if (! date.isValid()) {
            result.errors << QString("Row %1: Invalid date '%2'").arg(i + 2).arg(csvRow[dateCol]);
            result.failCount++;
            continue;
        }

        // Parse amount
        int amountCol = columnMapping.value("Amount", -1);
        if (amountCol < 0 || amountCol >= csvRow.size()) {
            result. errors << QString("Row %1: Amount column not mapped or out of range").arg(i + 2);
            result.failCount++;
            continue;
        }

        double rawAmount = parseAmount(csvRow[amountCol]);
        double amount = qAbs(rawAmount);

        if (amount == 0.0 && !csvRow[amountCol].trimmed().isEmpty()) {
            result. errors << QString("Row %1: Failed to parse amount '%2'").arg(i + 2).arg(csvRow[amountCol]);
            result.failCount++;
            continue;
        }

        // Determine type
        int typeCol = columnMapping.value("Type", -1);
        QString typeStr = (typeCol >= 0 && typeCol < csvRow. size()) ? csvRow[typeCol] : "";
        QString type = determineTransactionType(rawAmount, typeStr);

        // Get other fields
        int payeeCol = columnMapping.value("Payee", -1);
        QString payee = (payeeCol >= 0 && payeeCol < csvRow.size()) ? csvRow[payeeCol] : "";

        int descCol = columnMapping. value("Description", -1);
        QString description = (descCol >= 0 && descCol < csvRow.size()) ? csvRow[descCol] :  "";

        // Create transaction
        Transaction transaction;
        transaction.setAccountId(accountId);
        transaction.setDate(date);
        transaction.setAmount(amount);
        transaction.setType(type);
        transaction.setPayee(payee);
        transaction.setDescription(description);
        transaction.setCategoryId(0);

        // Calculate hash
        QString hash = transaction.calculateHash();

        result.transactionsToImport.append(transaction);
        result.hashes.append(hash);
    }

    bool allowDuplicates = ui->allowDuplicatesCheckBox->isChecked();
    bool allowPartialImport = ui->allowPartialImportCheckBox->isChecked();

    // Batch check for existing hashes (only if NOT allowing duplicates)
    if (!allowDuplicates) {
        qDebug() << "Checking" << result.hashes. size() << "hashes for duplicates... ";
        QList<QString> existingHashList = transactionDao.getExistingHashes(result.hashes);
        result.duplicateHashes = QSet<QString>(existingHashList. begin(), existingHashList.end());
        qDebug() << "Found" << result.duplicateHashes.size() << "existing duplicates";
    } else {
        qDebug() << "Duplicate checking disabled by user";
    }

    // ============================================================
    // START DATABASE TRANSACTION
    // ============================================================
    QSqlDatabase db = DatabaseManager:: instance().database();

    if (! db.transaction()) {
        result.errors << "Failed to start database transaction";
        result.failCount = result.transactionsToImport.size();
        qCritical() << "Failed to start transaction:" << db.lastError().text();
        return result;
    }

    qDebug() << "Started database transaction (partial import:"
             << (allowPartialImport ? "enabled" : "disabled") << ")";

    // Process transactions
    for (int i = 0; i < result.transactionsToImport.size(); ++i) {
        Transaction transaction = result.transactionsToImport[i];
        QString hash = result.hashes[i];

        // Check if duplicate (only if user hasn't allowed duplicates)
        if (!allowDuplicates && result. duplicateHashes.contains(hash)) {
            result.duplicateCount++;
            qDebug() << "Skipping duplicate:" << transaction.getDate()
                     << transaction.getAmount() << transaction.getPayee();
            continue;
        }

        // CREATE SAVEPOINT if partial import is allowed
        QSqlQuery savepointQuery(db);
        QString savepointName;
        if (allowPartialImport) {
            savepointName = QString("sp_%1").arg(i);
            if (! savepointQuery.exec("SAVEPOINT " + savepointName)) {
                qWarning() << "Failed to create savepoint:" << savepointQuery.lastError().text();
            }
        }

        // Try to add transaction
        int newId = transactionDao.addTransaction(transaction);

        if (newId != -1) {
            result.successCount++;
            qDebug() << (writeToDatabase ? "Imported" :  "Validated") << "transaction:"
                     << transaction. getDate() << transaction.getAmount()
                     << transaction.getType() << transaction.getPayee();

            // RELEASE SAVEPOINT (commit this transaction)
            if (allowPartialImport) {
                savepointQuery.exec("RELEASE " + savepointName);
            }
        } else {
            result.failCount++;
            QString errorMsg = QString("Row %1: Database error - %2")
                                   . arg(i + 2)
                                   .arg(db.lastError().text());
            result.errors << errorMsg;
            qWarning() << errorMsg;

            if (allowPartialImport) {
                // ROLLBACK TO SAVEPOINT (undo only this transaction)
                if (!savepointQuery.exec("ROLLBACK TO " + savepointName)) {
                    qWarning() << "Failed to rollback savepoint:" << savepointQuery. lastError().text();
                }
                // Release the savepoint after rollback
                savepointQuery.exec("RELEASE " + savepointName);

                qDebug() << "Rolled back failed transaction, continuing with next... ";
            } else {
                // NOT allowing partial import - abort entire transaction
                qWarning() << "Aborting entire import due to error (partial import disabled)";
                db.rollback();

                // All transactions failed
                result.errors. clear();
                result.errors << "Import aborted:  " + errorMsg;
                result.errors << "All transactions were rolled back (partial import is disabled)";
                result.failCount = result.transactionsToImport.size();
                result.successCount = 0;

                return result;
            }
        }
    }

    // ============================================================
    // COMMIT or ROLLBACK based on mode
    // ============================================================
    if (writeToDatabase) {
        if (db.commit()) {
            qDebug() << "Transaction COMMITTED - changes saved to database";
        } else {
            qCritical() << "Failed to commit transaction:" << db.lastError().text();
            db.rollback();
            result.errors << "Failed to commit changes to database";
            result.failCount += result.successCount;
            result.successCount = 0;
        }
    } else {
        // DRY RUN - rollback all changes
        if (db.rollback()) {
            qDebug() << "Transaction ROLLED BACK - no changes made to database (dry run)";
        } else {
            qWarning() << "Failed to rollback transaction:" << db.lastError().text();
        }
    }

    return result;
}

bool ImportManagerDialog::validateMapping()
{
    // Check that required fields are mapped
    if (! columnMapping.contains("Date") || columnMapping["Date"] < 0) {
        QMessageBox::warning(this, "Validation Error", "Date field must be mapped");
        return false;
    }

    if (!columnMapping.contains("Amount") || columnMapping["Amount"] < 0) {
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

void ImportManagerDialog::onImportClicked()
{
    qDebug() << "=== IMPORT MODE ===";

    ImportResult result = processTransactions(true);  // true = write to DB

    // Show detailed results
    QString message = QString("Import Results:\n\n");
    message += QString("✅ Successfully imported: %1 transactions\n").arg(result.successCount);

    if (result.duplicateCount > 0) {
        message += QString("⚠️  Skipped duplicates: %1 transactions\n").arg(result.duplicateCount);
    }

    if (result.failCount > 0) {
        message += QString("❌ Failed:  %1 transactions\n").arg(result.failCount);

        if (! result.errors.isEmpty()) {
            message += "\n\nErrors:";
            int errorCount = qMin(10, result.errors.size());
            for (int i = 0; i < errorCount; ++i) {
                message += "\n• " + result.errors[i];
            }
            if (result.errors.size() > 10) {
                message += QString("\n... and %1 more errors (check console)").arg(result.errors.size() - 10);
            }
        }

        QMessageBox::warning(this, "Import Complete with Errors", message);

        for (const QString &error : result. errors) {
            qWarning() << error;
        }
    } else {
        QMessageBox:: information(this, "Import Complete", message);
    }

    if (result.successCount > 0) {
        emit transactionsImported(result.successCount);
        accept();
    } else if (result.duplicateCount > 0 && result.failCount == 0) {
        QMessageBox::information(this, "Import Complete",
                                 "All transactions were duplicates and have been skipped.");
        reject();
    } else if (result.failCount > 0 && result.successCount == 0) {
        // All failed
        QMessageBox::critical(this, "Import Failed",
                              "No transactions were imported due to errors.");
        reject();
    }
}

void ImportManagerDialog::onCancelClicked()
{
    reject();
}
