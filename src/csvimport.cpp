#include "csvimport.h"
#include "csvparser.h"
#include "account.h"
#include "balance.h"
#include "category.h"
#include "db/databasemanager.h"

#include <QLocale>
#include <algorithm>
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
        {FieldBalance, {"balance", "running balance", "saldo", "saldo nach buchung", "kontostand"}},
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

QList<FileCategory> fileCategories(const QList<QStringList>& rows, const Mapping& mapping, const Options& options) {
    QList<FileCategory> result;
    if (!mapping.contains(FieldCategory)) {
        return result;
    }
    QHash<QString, int> indexByKey;
    for (const QStringList& row : rows) {
        const QString name = column(row, mapping, FieldCategory).simplified();
        if (name.isEmpty()) continue;
        const QString key = name.toLower();
        if (!indexByKey.contains(key)) {
            indexByKey.insert(key, int(result.size()));
            result.append({name, 0, 0});
        }
        FileCategory& category = result[indexByKey.value(key)];
        category.rows++;
        bool ok = false;
        const Cents amount = parseAmount(column(row, mapping, FieldAmount), options.decimalSeparator, &ok);
        if (ok && transactionType(amount, column(row, mapping, FieldType), options.interpretation) == "expense") {
            category.expenseRows++;
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const FileCategory& a, const FileCategory& b) { return a.rows > b.rows; });
    return result;
}

StatedBalance statedBalance(const QList<QStringList>& rows, const Mapping& mapping, QChar decimalSeparator) {
    StatedBalance result;
    if (!mapping.contains(FieldBalance) || !mapping.contains(FieldDate)) {
        return result;
    }
    // Banks list newest first or oldest first. On the newest day, the row furthest in that
    // direction carries the balance after all of the day's bookings.
    QList<QPair<QDate, Cents>> balances;
    for (const QStringList& row : rows) {
        bool ok = false;
        const QDate date = parseDate(hasColumn(row, mapping, FieldDate) ? column(row, mapping, FieldDate) : QString());
        const QString text = hasColumn(row, mapping, FieldBalance) ? column(row, mapping, FieldBalance) : QString();
        const Cents cents = parseAmount(text, decimalSeparator, &ok);
        if (date.isValid() && ok && !text.trimmed().isEmpty()) {
            balances.append({date, cents});
        }
    }
    if (balances.isEmpty()) {
        return result;
    }
    const bool newestFirst = balances.first().first > balances.last().first;
    result.found = true;
    result.date = balances.first().first;
    result.cents = balances.first().second;
    for (const auto& balance : balances) {
        // Oldest first: a later row of the same day wins; newest first: the first row stays
        if (balance.first > result.date || (!newestFirst && balance.first == result.date)) {
            result.date = balance.first;
            result.cents = balance.second;
        }
    }
    return result;
}

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

    const QString categoryName = column(row, mapping, FieldCategory).simplified().toLower();

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
    QStringList categoryKeys;                        // Lower-case category of each transaction, for new categories
    for (int i = 0; i < rows.size(); ++i) {
        const RowResult row = buildTransaction(rows[i], i + 2, mapping, options);
        if (!row.ok) {
            result.failed++;
            result.errors << row.error;
            continue;
        }
        transactions.append(row.transaction);
        hashes.append(row.transaction.calculateHash());
        categoryKeys.append(column(rows[i], mapping, FieldCategory).simplified().toLower());
    }

    // Without partial import, one unreadable row stops the whole import
    if (result.failed > 0 && !runOptions.allowPartialImport) {
        result.errors.prepend("Import aborted: some rows couldn't be read (partial import is disabled)");
        result.failed = rows.size();
        return result;
    }

    // The balance is checked on the newest day of the file unless the caller picks one
    result.checkDate = runOptions.checkDate;
    for (const Transaction& transaction : transactions) {
        if (!runOptions.checkDate.isValid() && (!result.checkDate.isValid() || transaction.getDate() > result.checkDate)) {
            result.checkDate = transaction.getDate();
        }
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
    if (result.checkDate.isValid()) {
        result.balanceBeforeCents = Balance::onDate(options.accountId, result.checkDate);
    }

    // Create the file's new categories (rolled back with everything else in a dry run) and use them
    for (auto it = options.categoriesToCreate.cbegin(); it != options.categoriesToCreate.cend(); ++it) {
        const int categoryId = CategoryDAO(AccountDAO::ownerOf(options.accountId)).addCategory(Category(-1, it.value().name, it.value().type));
        if (categoryId <= 0) {
            db.rollback();
            result.errors = {QString("Import aborted: the category \"%1\" couldn't be created").arg(it.value().name)};
            result.failed = rows.size();
            return result;
        }
        result.categoriesCreated++;
        for (int i = 0; i < transactions.size(); ++i) {
            if (categoryKeys[i] == it.key()) transactions[i].setCategoryId(categoryId);
        }
    }

    // Rules fill in rows without a category (or replace the file's when allowed) and rename payees.
    // The duplicate hashes above were taken before, from the bank's names, and a renamed transaction
    // keeps the bank's name as its original payee, so its stored hash is the same.
    for (int i = 0; i < transactions.size() && !options.rules.isEmpty(); ++i) {
        const bool isNew = !existing.contains(hashes[i]);
        const int current = transactions[i].getCategoryId();
        if (current <= 0 || options.rulesOverrideFileCategories) {
            const std::optional<int> category = Rules::categoryFor(options.rules, transactions[i]);
            if (category && *category != current) {
                transactions[i].setCategoryId(*category);
                if (isNew) result.categorisedByRules++;
            }
        }
        const Rule* rename = Rules::renameRule(options.rules, transactions[i]);
        if (rename && rename->newPayee != transactions[i].getPayee()) {
            transactions[i].renamePayee(rename->newPayee);
            if (isNew) result.renamedByRules++;
        }
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
            result.importedNetCents += transactions[i].getType() == "income" ? transactions[i].getAmountCents()
                                                                               : -transactions[i].getAmountCents();
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

    // Balance after the import, and a correction to reach the expected one
    if (result.checkDate.isValid()) {
        result.balanceAfterCents = Balance::onDate(options.accountId, result.checkDate);
        if (runOptions.correct && runOptions.correctTo != result.balanceAfterCents) {
            const Cents difference = runOptions.correctTo - result.balanceAfterCents;
            if (Balance::addCorrection(options.accountId, result.checkDate, difference, runOptions.correctionNote) <= 0) {
                db.rollback();
                result.errors = {"Import aborted: the balance correction couldn't be saved"};
                result.failed = rows.size();
                result.imported = 0;
                result.duplicates = 0;
                return result;
            }
            result.correctionCents = difference;
        }
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
