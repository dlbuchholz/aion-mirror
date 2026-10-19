#ifndef IMPORTMANAGERDIALOG_H
#define IMPORTMANAGERDIALOG_H

#include <QDialog>
#include <QHash>
#include <QMap>
#include "csvimport.h"

class QCheckBox;
class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;

// Import wizard for CSV bank exports: file and account, which column is what, the file's
// categories, then a review with a balance check. The import logic lives in CsvImport.
class ImportManagerDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Step { File, Columns, Categories, Review };

    explicit ImportManagerDialog(int userId, QWidget *parent = nullptr);

    void loadFile(const QString &filePath);          // Same as choosing it with "Choose file"
    Step currentStep() const;

signals:
    void transactionsImported(int count);

private:
    // Pages
    QWidget *buildFilePage();
    QWidget *buildColumnsPage();
    QWidget *buildCategoriesPage();
    QWidget *buildReviewPage();
    QLabel *pageTitle(const QString &text, QWidget *parent);
    QLabel *hintLabel(const QString &text, QWidget *parent);

    // Navigation
    void goTo(Step step);
    void next();
    void back();
    bool hasCategoriesStep() const;
    void updateStepIndicator();
    void showError(QLabel *label, const QString &message);

    // File and columns
    void chooseFile();
    void parseFile();
    void updateFormatSummary();
    void applyMapping(const CsvImport::Mapping &mapping);
    CsvImport::Mapping mapping() const;
    void updateExamples();
    CsvImport::Options importOptions() const;

    // Categories named in the file: matched, created on import, mapped to another or dropped
    void refreshFileCategories();
    void applyCategoryChoices(CsvImport::Options &options) const;
    void updateCategorySummary();
    void setAllNewCategories(bool create);          // false: import them without a category

    // Review: dry run of the import, compared with the balance the bank shows
    void refreshReview();
    void updateBalanceStatus();
    bool expectedBalance(Cents *cents) const;      // false if the field is empty or unreadable
    CsvImport::Result runImport(bool dryRun);
    void import();

    int userId;
    QString filePath;
    QString fileText;
    QChar delimiter = ',';
    QChar decimalSeparator = '.';
    QList<QStringList> rows;                         // Without the header
    QStringList headers;

    QStackedWidget *pages = nullptr;
    QList<QLabel *> stepLabels;
    QPushButton *backButton = nullptr;
    QPushButton *nextButton = nullptr;
    QPushButton *importButton = nullptr;

    // File page
    QLineEdit *filePathEdit = nullptr;
    QComboBox *accountCombo = nullptr;
    QLabel *formatSummaryLabel = nullptr;
    QToolButton *changeFormatButton = nullptr;
    QFrame *formatBox = nullptr;
    QComboBox *delimiterCombo = nullptr;
    QComboBox *decimalCombo = nullptr;
    QLabel *fileErrorLabel = nullptr;

    // Columns page
    QLabel *columnsSubtitle = nullptr;
    QMap<QString, QComboBox *> mappingCombos;       // CsvImport field -> column choice
    QMap<QString, QLabel *> exampleLabels;
    QToolButton *moreOptionsButton = nullptr;
    QFrame *moreOptionsBox = nullptr;
    QComboBox *interpretationCombo = nullptr;
    QCheckBox *allowDuplicatesCheck = nullptr;
    QCheckBox *allowPartialCheck = nullptr;
    QLabel *columnsErrorLabel = nullptr;

    // Categories page
    QList<CsvImport::FileCategory> fileCategories;
    QScrollArea *categoriesScroll = nullptr;
    QLabel *categorySummaryLabel = nullptr;
    QToolButton *newCategoriesButton = nullptr;
    QHash<QString, QComboBox *> categoryChoices;     // Lower-case file name -> choice (unknown names only)
    QHash<QString, QString> chosenCategories;        // Remembered while the table is rebuilt
    int matchedCategories = 0;

    // Review page
    CsvImport::Result dryRun;
    QLabel *reviewTitle = nullptr;
    QLabel *newCountLabel = nullptr;
    QLabel *duplicateCountLabel = nullptr;
    QLabel *failedCountLabel = nullptr;
    QLabel *problemsLabel = nullptr;
    QLabel *newCategoriesLabel = nullptr;
    QLabel *balanceAfterCaption = nullptr;
    QLabel *balanceAfterValue = nullptr;
    QLabel *balanceBreakdownLabel = nullptr;        // "Now … · from this file …"
    QFrame *balanceBox = nullptr;
    QLabel *expectedCaption = nullptr;
    QLineEdit *expectedEdit = nullptr;
    QLabel *expectedSourceLabel = nullptr;
    QLabel *balanceStatusLabel = nullptr;
    QCheckBox *correctionCheckBox = nullptr;
    bool expectedEditedByUser = false;
    bool closeOnly = false;                          // Nothing to import or correct: the button just closes
};

#endif // IMPORTMANAGERDIALOG_H
