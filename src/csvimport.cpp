#include "csvimport.h"
#include "csvparser.h"
#include "db/databasemanager.h"

#include <QLocale>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>

namespace CsvImport {

QChar detectDelimiter(const QString& text) {
    if (text.isEmpty()) {
        return ',';
    }

    // The delimiter that splits every record into the same, largest number of columns wins
    QChar best = ',';
    int bestColumns = 1;
    for (QChar delimiter : {QChar(','), QChar(';'), QChar('\t'), QChar('|')}) {
        QString error;
        const QList<QStringList> records = CsvParser::parse(text, delimiter, &error);
        if (!error.isEmpty() || records.isEmpty() || records.first().size() <= 1) {
            continue;
        }
        const int columns = records.first().size();
        bool consistent = true;
        for (const QStringList& record : records) {
            if (record.size() != columns) {
                consistent = false;
                break;
            }
        }
        if (consistent && columns > bestColumns) {
            bestColumns = columns;
            best = delimiter;
        }
    }
    return best;
}

QChar detectDecimalSeparator(const QList<QStringList>& rows, int amountColumn, QChar fallback) {
    // Look at whole numbers like "1.234,56" and vote with the last separator in them.
    // Exactly three digits after it ("1,234") is more likely a thousands separator, so it doesn't vote.
    static const QRegularExpression numberPattern("\\d[\\d.,]*[.,](\\d+)");
    int commaVotes = 0;
    int pointVotes = 0;

    for (const QStringList& row : rows) {
        QStringList values;
        if (amountColumn >= 0) {
            if (amountColumn < row.size()) values.append(row[amountColumn]);
        } else {
            values = row;
        }
        for (const QString& value : values) {
            const QRegularExpressionMatch match = numberPattern.match(value);
            if (!match.hasMatch() || match.captured(1).size() == 3) {
                continue;
            }
            const QString number = match.captured(0);
            if (number.lastIndexOf(',') > number.lastIndexOf('.')) {
                ++commaVotes;
            } else {
                ++pointVotes;
            }
        }
    }

    if (commaVotes > pointVotes) return ',';
    if (pointVotes > commaVotes) return '.';
    return fallback;
}

Mapping autoMapColumns(const QStringList& headers) {
    // Header names (English and German bank exports), in the order fields claim columns
    const QList<QPair<QString, QStringList>> knownNames = {
        {FieldDate, {"date", "transaction date", "datum", "trans. date", "buchungstag", "valutadatum", "wertstellung"}},
        {FieldAmount, {"amount", "value", "betrag", "sum", "wert", "umsatz"}},
        {FieldPayee, {"payee", "merchant", "empfänger", "recipient", "name zahlungsbeteiligter", "auftraggeber",
                      "zahlungsempfänger", "begünstigter", "name"}},
        {FieldDescription, {"description", "memo", "note", "beschreibung", "details", "verwendungszweck",
                            "verwendung", "zweck", "referenz"}},
        {FieldType, {"type", "transaction type", "typ", "art", "soll/haben", "buchungsart"}},
        {FieldCategory, {"category", "kategorie", "transaktionsart"}},
    };

    Mapping mapping;
    QSet<int> usedColumns;
    for (const auto& field : knownNames) {
        for (int column = 0; column < headers.size(); ++column) {
            if (!usedColumns.contains(column) && field.second.contains(headers[column].trimmed().toLower())) {
                mapping.insert(field.first, column);
                usedColumns.insert(column);
                break;
            }
        }
    }
    return mapping;
}

QDate parseDate(const QString& text) {
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) {
        return QDate();
    }

    // Day-first formats come before month-first ones, so "03/04/2024" is 3 April
    static const QStringList formats = {
        "yyyy-MM-dd", "dd/MM/yyyy", "MM/dd/yyyy", "dd.MM.yyyy", "yyyy/MM/dd",
        "d/M/yyyy", "d.M.yyyy", "dd-MM-yyyy", "MM-dd-yyyy", "yyyyMMdd",
        "dd MMM yyyy", "MMM dd, yyyy"
    };
    for (const QString& format : formats) {
        const QDate date = QDate::fromString(cleaned, format);
        if (date.isValid()) {
            return date;
        }
    }
    return QLocale().toDate(cleaned, QLocale::ShortFormat);
}

Cents parseAmount(const QString& text, QChar decimalSeparator, bool* ok) {
    if (ok) *ok = false;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    // "(12.50)" is accounting notation for -12.50
    const bool negative = trimmed.contains('(') || trimmed.contains('-');

    // Keep digits and separators only (drops currency symbols, spaces, signs)
    QString cleaned = trimmed;
    static const QRegularExpression nonNumeric("[^0-9.,]");
    cleaned.remove(nonNumeric);
    if (decimalSeparator == ',') {
        cleaned.remove('.');                         // 1.234,56 -> 1234,56
        cleaned.replace(',', '.');
    } else {
        cleaned.remove(',');                         // 1,234.56 -> 1234.56
    }

    // Exact decimal parsing, no detour through double
    const std::optional<Cents> value = Money::parsePlain(cleaned);
    if (!value) {
        return 0;
    }
    if (ok) *ok = true;
    return negative ? -*value : *value;
}

QString transactionType(Cents amount, const QString& typeText, AmountInterpretation interpretation) {
    switch (interpretation) {
    case AmountInterpretation::SignBasedInverted:
        return amount < 0 ? "income" : "expense";
    case AmountInterpretation::AllIncome:
        return "income";
    case AmountInterpretation::AllExpense:
        return "expense";
    case AmountInterpretation::ColumnBased: {
        const QString lower = typeText.toLower();
        if (lower.contains("income") || lower.contains("credit")) return "income";
        if (lower.contains("expense") || lower.contains("debit")) return "expense";
        break;                                       // Unknown text: use the sign
    }
    case AmountInterpretation::SignBased:
        break;
    }
    return amount < 0 ? "expense" : "income";
}

namespace {

QString column(const QStringList& row, const Mapping& mapping, const QString& field) {
    const int index = mapping.value(field, -1);
    return (index >= 0 && index < row.size()) ? row[index] : QString();
}

bool hasColumn(const QStringList& row, const Mapping& mapping, const QString& field) {
    const int index = mapping.value(field, -1);
    return index >= 0 && index < row.size();
}

} // namespace

RowResult buildTransaction(const QStringList& row, int rowNumber, const Mapping& mapping, const Options& options) {
    RowResult result;
    const QString prefix = QString("Row %1: ").arg(rowNumber);

    if (!hasColumn(row, mapping, FieldDate)) {
        result.error = prefix + "Date column not mapped or out of range";
        return result;
    }
    const QDate date = parseDate(column(row, mapping, FieldDate));
    if (!date.isValid()) {
        result.error = prefix + QString("Invalid date '%1'").arg(column(row, mapping, FieldDate));
        return result;
    }

    if (!hasColumn(row, mapping, FieldAmount)) {
        result.error = prefix + "Amount column not mapped or out of range";
        return result;
    }
    const QString amountText = column(row, mapping, FieldAmount);
    bool amountOk = false;
    const Cents signedAmount = parseAmount(amountText, options.decimalSeparator, &amountOk);
    if (amountText.trimmed().isEmpty()) {
        result.error = prefix + "Amount is missing";
        return result;
    }
    if (!amountOk) {
        result.error = prefix + QString("Failed to parse amount '%1'").arg(amountText);
        return result;
    }
    if (signedAmount == 0) {
        result.error = prefix + "Amount is zero";
        return result;
    }

    const QString categoryName = column(row, mapping, FieldCategory).trimmed().toLower();

    result.transaction.setAccountId(options.accountId);
    result.transaction.setDate(date);
    result.transaction.setAmountCents(qAbs(signedAmount));
    result.transaction.setType(transactionType(signedAmount, column(row, mapping, FieldType), options.interpretation));
    result.transaction.setPayee(column(row, mapping, FieldPayee));
    result.transaction.setDescription(column(row, mapping, FieldDescription));
    result.transaction.setCategoryId(options.categoryIds.value(categoryName, 0));
    result.ok = true;
    return result;
}

Result run(const QList<QStringList>& rows, const Mapping& mapping, const Options& options, const RunOptions& runOptions) {
    Result result;

    // Read every row first; the file's first line is the header, so data starts at line 2
    QList<Transaction> transactions;
    QStringList hashes;
    for (int i = 0; i < rows.size(); ++i) {
        const RowResult row = buildTransaction(rows[i], i + 2, mapping, options);
        if (!row.ok) {
            result.failed++;
            result.errors << row.error;
            continue;
        }
        transactions.append(row.transaction);
        hashes.append(row.transaction.calculateHash());
    }

    // Without partial import, one unreadable row stops the whole import
    if (result.failed > 0 && !runOptions.allowPartialImport) {
        result.errors.prepend("Import aborted: some rows couldn't be read (partial import is disabled)");
        result.failed = rows.size();
        return result;
    }

    TransactionDAO transactionDao;
    QSet<QString> existing;
    if (!runOptions.allowDuplicates) {
        const QList<QString> found = transactionDao.getExistingHashes(hashes);
        existing = QSet<QString>(found.begin(), found.end());
    }

    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        result.errors << "Failed to start database transaction";
        result.failed += transactions.size();
        return result;
    }

    for (int i = 0; i < transactions.size(); ++i) {
        if (existing.contains(hashes[i])) {
            result.duplicates++;
            continue;
        }

        // With partial import, a savepoint lets one failing row be undone on its own
        QSqlQuery savepoint(db);
        const QString savepointName = QString("sp_%1").arg(i);
        if (runOptions.allowPartialImport) {
            savepoint.exec("SAVEPOINT " + savepointName);
        }

        if (transactionDao.addTransaction(transactions[i]) != -1) {
            result.imported++;
            if (runOptions.allowPartialImport) {
                savepoint.exec("RELEASE " + savepointName);
            }
            continue;
        }

        const QString error = QString("Database error - %1").arg(db.lastError().text());
        if (runOptions.allowPartialImport) {
            result.failed++;
            result.errors << error;
            savepoint.exec("ROLLBACK TO " + savepointName);
            savepoint.exec("RELEASE " + savepointName);
            continue;
        }

        db.rollback();
        result.errors = {"Import aborted: " + error, "All transactions were rolled back (partial import is disabled)"};
        result.failed = rows.size();
        result.imported = 0;
        result.duplicates = 0;
        return result;
    }

    if (runOptions.dryRun) {
        db.rollback();
    } else if (!db.commit()) {
        db.rollback();
        result.errors << "Failed to commit changes to database";
        result.failed += result.imported;
        result.imported = 0;
    }
    return result;
}

} // namespace CsvImport
