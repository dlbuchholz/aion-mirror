#include "csvparser.h"

QList<QStringList> CsvParser::parse(const QString& text, QChar delimiter, QString* error) {
    QList<QStringList> records;
    QStringList record;
    QString field;
    bool quoted = false;

    if (error) error->clear();

    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field += ch;
            }
            continue;
        }

        if (ch == '"' && field.isEmpty()) {
            quoted = true;
        } else if (ch == delimiter) {
            record.append(field);
            field.clear();
        } else if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            record.append(field);
            field.clear();
            records.append(record);
            record.clear();
        } else {
            field += ch;
        }
    }

    if (quoted) {
        if (error) *error = "Unterminated quoted CSV field";
        return {};
    }
    if (!field.isEmpty() || !record.isEmpty()) {
        record.append(field);
        records.append(record);
    }
    return records;
}
