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
};

Format formatOf(const QString& path);                // By the file name; None for anything else
QString formatName(Format format);                   // "Excel", "LibreOffice"

// The sheets in the workbook's order; on failure none, and error says why
QList<Sheet> read(const QByteArray& data, Format format, QString* error);
QList<Sheet> readFile(const QString& path, QString* error);

} // namespace Spreadsheet

#endif // SPREADSHEET_H
