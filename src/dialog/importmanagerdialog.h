#ifndef IMPORTMANAGERDIALOG_H
#define IMPORTMANAGERDIALOG_H

#include <QDialog>
#include <QMap>
#include <QComboBox>
#include <QTableWidget>
#include "csvimport.h"

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QTimer;
class QScrollArea;
class QToolButton;

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

    void loadFile(const QString &filePath);          // Same as choosing it with "Select file"

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
    void makeScrollable();                          // Everything but the buttons scrolls, so they stay reachable
    void setupColumnMappingTable();
    void populateAccountComboBox();
    void updatePreviewTable();
    void applyMappingToComboBoxes(const CsvImport::Mapping &mapping);
    CsvImport::Mapping mappingFromComboBoxes() const;

    // Import
    bool validateMapping();
    CsvImport::Result runImport(bool dryRun);

    // Categories named in the file: matched, created on import, mapped to another or dropped
    void refreshFileCategories();
    void applyCategoryChoices(CsvImport::Options &options) const;
    void updateCategorySummary();
    void setAllNewCategories(bool create);          // false: import them without a category

    // Balance check: dry run of the import, compared with the balance the bank shows
    void setupBalanceCheck();
    void scheduleBalanceCheck();
    void refreshBalanceCheck();
    void updateBalanceStatus();
    bool expectedBalance(Cents* cents) const;      // false if the field is empty or unreadable
    bool correctionWanted() const;

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

    // Categories in the file
    QGroupBox* categoriesGroupBox = nullptr;
    QWidget* categoriesTable = nullptr;
    QScrollArea* categoriesScroll = nullptr;
    QLabel* categorySummaryLabel = nullptr;
    QToolButton* newCategoriesButton = nullptr;
    int matchedCategories = 0;
    QHash<QString, QComboBox*> categoryChoices;      // Lower-case file name -> choice (unknown names only)
    QHash<QString, QString> chosenCategories;        // Remembered while the table is rebuilt

    // Balance check
    QTimer* balanceTimer = nullptr;
    CsvImport::Result balanceResult;
    bool balanceReady = false;
    bool expectedEditedByUser = false;
    QGroupBox* balanceGroupBox = nullptr;
    QLabel* balanceHintLabel = nullptr;
    QWidget* balanceDetails = nullptr;
    QLabel* balanceNowCaption = nullptr;
    QLabel* balanceNowValue = nullptr;
    QLabel* balanceChangeCaption = nullptr;
    QLabel* balanceChangeValue = nullptr;
    QLabel* balanceAfterCaption = nullptr;
    QLabel* balanceAfterValue = nullptr;
    QLabel* expectedCaption = nullptr;
    QLineEdit* expectedEdit = nullptr;
    QLabel* expectedSourceLabel = nullptr;
    QLabel* balanceStatusLabel = nullptr;
    QCheckBox* correctionCheckBox = nullptr;
};

#endif // IMPORTMANAGERDIALOG_H
