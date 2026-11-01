#ifndef CSVIMPORT_H
#define CSVIMPORT_H

#include <QChar>
#include <QDate>
#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include "rule.h"
#include "transaction.h"

// CSV import logic without UI: reading values, detecting the file's conventions,
// mapping columns and writing transactions. ImportManagerDialog is the UI on top.
namespace CsvImport {

// Fields a CSV column can be mapped to (also the keys of a Mapping)
inline const QString FieldDate = QStringLiteral("Date");
inline const QString FieldAmount = QStringLiteral("Amount");
inline const QString FieldPayee = QStringLiteral("Payee");
inline const QString FieldDescription = QStringLiteral("Description");
inline const QString FieldCategory = QStringLiteral("Category");
inline const QString FieldType = QStringLiteral("Type");
// Only for the balance check: the bank's running balance after each row. Never saved.
inline const QString FieldBalance = QStringLiteral("Balance");

using Mapping = QMap<QString, int>;                  // Field -> CSV column index

enum class AmountInterpretation {
    SignBased,                                       // Negative = expense, positive = income
    SignBasedInverted,                               // Negative = income, positive = expense
    AllIncome,
    AllExpense,
    ColumnBased                                      // Use the mapped Type column (falls back to the sign)
};

// A file's text: by its byte order mark, else UTF-8, else Windows-1252 (older bank software)
QString decodeText(const QByteArray& bytes);

// Detection of the file's conventions
QChar detectDelimiter(const QString& text);          // ',', ';', '\t' or '|'
QChar detectDecimalSeparator(const QList<QStringList>& rows, int amountColumn, QChar fallback);
Mapping autoMapColumns(const QStringList& headers);

// Reading single values
QDate parseDate(const QString& text);
Cents parseAmount(const QString& text, QChar decimalSeparator, bool* ok = nullptr);   // Signed cents
QString transactionType(Cents amount, const QString& typeText, AmountInterpretation interpretation);

struct Options {
    int accountId = -1;
    QChar decimalSeparator = '.';
    AmountInterpretation interpretation = AmountInterpretation::SignBased;
    QHash<QString, int> categoryIds;                 // Lower-case category name -> ID
    // Categories the file names that don't exist yet and should be created on import
    struct NewCategory {
        QString name;                                // As spelled in the file
        QString type;                                // "expense" or "income"
    };
    QHash<QString, NewCategory> categoriesToCreate;  // Lower-case name -> new category
    // Rules give the rows a category (see rule.h). They only fill in rows without one, unless
    // rulesOverrideFileCategories lets them replace the category from the file.
    QList<Rule> rules;
    bool rulesOverrideFileCategories = false;
};

// One CSV row as a transaction (rowNumber is the line in the file, for messages)
struct RowResult {
    bool ok = false;
    Transaction transaction;
    QString error;
};
RowResult buildTransaction(const QStringList& row, int rowNumber, const Mapping& mapping, const Options& options);

struct Result {
    int imported = 0;                                // Written (or, in a dry run, would be written)
    int duplicates = 0;                              // Skipped because they already exist
    int failed = 0;
    QStringList errors;

    // Balance check on checkDate (the latest date in the file unless RunOptions says otherwise)
    QDate checkDate;
    Cents importedNetCents = 0;                      // Income minus expenses of the imported rows
    Cents balanceBeforeCents = 0;                    // Account balance on checkDate without the imported rows
    Cents balanceAfterCents = 0;                     // ... and with them
    Cents correctionCents = 0;                       // Balance correction added (see RunOptions::correctTo)
    int categoriesCreated = 0;
    int categorisedByRules = 0;                      // Imported rows whose category came from a rule
    int renamedByRules = 0;                          // Imported rows whose payee a rule renamed
};

// The distinct values of the mapped Category column, most used first
struct FileCategory {
    QString name;                                    // First spelling found in the file
    int rows = 0;
    int expenseRows = 0;
    QString likelyType() const { return expenseRows * 2 >= rows ? "expense" : "income"; }
};
QList<FileCategory> fileCategories(const QList<QStringList>& rows, const Mapping& mapping, const Options& options);

// The bank's balance from the mapped Balance column: the one after the newest row
struct StatedBalance {
    bool found = false;
    QDate date;
    Cents cents = 0;
};
StatedBalance statedBalance(const QList<QStringList>& rows, const Mapping& mapping, QChar decimalSeparator);

struct RunOptions {
    bool allowDuplicates = false;
    bool allowPartialImport = false;                 // Keep the valid rows when some fail
    bool dryRun = false;                             // Everything is rolled back at the end
    QDate checkDate;                                 // Invalid = the latest date in the file
    bool correct = false;                            // Add a balance correction so the balance on checkDate ...
    Cents correctTo = 0;                             // ... becomes this
    QString correctionNote;
    QStringList rowLabels;                           // Where each row is, for messages ("Februar, row 3"); empty: "Row 2" on
};

// Imports all rows in one database transaction
Result run(const QList<QStringList>& rows, const Mapping& mapping, const Options& options, const RunOptions& runOptions);

} // namespace CsvImport

#endif // CSVIMPORT_H
