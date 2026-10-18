#ifndef IMPORTMANAGERDIALOG_H
#define IMPORTMANAGERDIALOG_H

#include <QDialog>
#include <QMap>
#include <QComboBox>
#include <QTableWidget>
#include "csvimport.h"

#include "../../ui/ui_importmanagerdialog.h"

namespace Ui {
class ImportManagerDialog;
}

// UI for importing a CSV file; the parsing and import logic lives in CsvImport
class ImportManagerDialog : public QDialog
{
    Q_OBJECT

public:
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
    void parseCSV(const QString &filePath);

    // UI setup
    void setupUI();
    void setupColumnMappingTable();
    void populateAccountComboBox();
    void updatePreviewTable();
    void applyMappingToComboBoxes(const CsvImport::Mapping &mapping);
    CsvImport::Mapping mappingFromComboBoxes() const;

    // Import
    bool validateMapping();
    CsvImport::Result runImport(bool dryRun);

    Ui::ImportManagerDialog *ui;

    int userId;
    QString currentFilePath;
    QChar csvDelimiter;
    QChar decimalSeparator;
    CsvImport::AmountInterpretation amountInterpretation;
    bool autoDetectDelimiterOnNextParse;

    // CSV data storage
    QList<QStringList> csvData;  // Rows without the header
    QStringList csvHeaders;      // Column headers from CSV

    // Available transaction fields (CsvImport field names plus "(Ignore)")
    QStringList transactionFields;

    // Widgets for column mapping
    QMap<QString, QComboBox*> mappingComboBoxes;
};

#endif // IMPORTMANAGERDIALOG_H
