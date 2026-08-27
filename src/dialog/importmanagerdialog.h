#ifndef IMPORTMANAGERDIALOG_H
#define IMPORTMANAGERDIALOG_H

#include <QDialog>
#include <QMap>
#include <QComboBox>
#include <QTableWidget>
#include "transaction.h"

#include "../../ui/ui_importmanagerdialog.h"

namespace Ui {
class ImportManagerDialog;
}

class ImportManagerDialog : public QDialog
{
    Q_OBJECT

public:
    enum class FileFormat {
        CSV,
        JSON,
        OFX,      // Open Financial Exchange (for future)
        QIF       // Quicken Interchange Format (for future)
    };

    enum class AmountInterpretation {
        SignBased,           // Negative = expense, Positive = income
        SignBasedInverted,   // Negative = income, Positive = expense
        AllIncome,           // All transactions are income
        AllExpense,          // All transactions are expense
        ColumnBased          // Use a separate column to determine type
    };

    explicit ImportManagerDialog(int userId, QWidget *parent = nullptr);
    ~ImportManagerDialog();

signals:
    void transactionsImported(int count);

private slots:
    void onSelectFileClicked();
    void onDelimiterChanged(const QString &delimiter);
    void onDecimalSeparatorChanged(int index);
    void onAmountInterpretationChanged(int index);
    void onColumnMappingChanged();
    void onPreviewRefreshClicked();
    void onDryRunClicked();
    void onImportClicked();
    void onCancelClicked();

private:
    // File handling
    void loadFile(const QString &filePath);
    void parseCSV(const QString &filePath);
    void parseFutureFormat(const QString &filePath);

    // UI setup
    void setupUI();
    void setupColumnMappingTable();
    void populateAccountComboBox();
    void updatePreviewTable();
    void autoMapColumns();

    // Data processing
    QChar detectCsvDelimiter(const QString &filePath);
    QChar detectDecimalSeparator(const QList<QStringList>& sampleData, int amountColumnIndex = -1);
    QList<Transaction> parseTransactionsFromPreview();
    QString determineTransactionType(double amount, const QString &typeColumn);
    QDate parseDate(const QString &dateStr);
    double parseAmount(const QString &amountStr);
    struct ImportResult;
    ImportResult processTransactions(bool writeToDatabase);
    bool validateMapping();

    Ui::ImportManagerDialog *ui;

    int userId;
    FileFormat currentFormat;
    QString currentFilePath;
    QChar csvDelimiter;
    QChar decimalSeparator;
    AmountInterpretation amountInterpretation;
    bool autoDetectDelimiterOnNextParse;

    // CSV data storage
    QList<QStringList> csvData;  // Raw CSV data
    QStringList csvHeaders;      // Column headers from CSV

    // Column mapping:   Our field name -> CSV column index
    QMap<QString, int> columnMapping;

    // Available transaction fields
    QStringList transactionFields;

    // Widgets for column mapping
    QMap<QString, QComboBox*> mappingComboBoxes;

    struct ImportResult {
        QList<Transaction> transactionsToImport;
        QList<QString> hashes;
        QSet<QString> duplicateHashes;
        int successCount;
        int duplicateCount;
        int failCount;
        QStringList errors;
    };
};

#endif // IMPORTMANAGERDIALOG_H
