#ifndef CSVPARSER_H
#define CSVPARSER_H

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

class CsvParser {
public:
    static QList<QStringList> parse(const QString& text, QChar delimiter, QString* error = nullptr);
};

#endif
