#ifndef SPREADSHEET_H
#define SPREADSHEET_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Reads the cell values of Excel (.xlsx) and LibreOffice (.ods) spreadsheets as rows of text,
// like a CSV file gives them: dates as yyyy-MM-dd, numbers with a point and without grouping,
// formulas as the value last calculated. Empty rows are left out.
namespace Spreadsheet {

enum class Format { None, Xlsx, Ods };

struct Sheet {
    QString name;
    QList<QStringList> rows;
    QList<int> rowNumbers;                           // Each row's number in the sheet (from 1), for messages
};

Format formatOf(const QString& path);                // By the file name; None for anything else
QString formatName(Format format);                   // "Excel", "LibreOffice"

// The sheets in the workbook's order; on failure none, and error says why
QList<Sheet> read(const QByteArray& data, Format format, QString* error);
QList<Sheet> readFile(const QString& path, QString* error);

// How another sheet's columns line up with the header row of the first: by name, ignoring case
// and spaces; a column without a name falls back to its position
struct ColumnMatch {
    bool hasHeader = false;                          // Its first row names columns of the header
    QList<int> columns;                              // For each header column, the sheet's column; -1 = none
    QStringList missing;                             // Header names the sheet doesn't have
    bool sameOrder = true;
};
ColumnMatch matchColumns(const QStringList& header, const QStringList& sheetHeader);

// A row of that sheet, in the header's column order
QStringList arrange(const QStringList& row, const ColumnMatch& match);

} // namespace Spreadsheet

#endif // SPREADSHEET_H
