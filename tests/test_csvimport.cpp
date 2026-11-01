#include <QtTest>
#include <QComboBox>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSqlQuery>
#include <QTimer>

#include "account.h"
#include "category.h"
#include "csvimport.h"
#include "dialog/importmanagerdialog.h"
#include "tests/test_db_fixture.h"

using namespace CsvImport;

class TestCsvImport : public QObject {
    Q_OBJECT

    TestDbFixture db;
    int accountId = -1;

    static Mapping basicMapping() {
        return {{FieldDate, 0}, {FieldAmount, 1}, {FieldPayee, 2}, {FieldDescription, 3}};
    }

    Options basicOptions() const {
        Options options;
        options.accountId = accountId;
        options.decimalSeparator = ',';
        return options;
    }

    static int transactionCount(int account) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT COUNT(*) FROM transactions WHERE account_id = :id");
        query.bindValue(":id", account);
        return query.exec() && query.next() ? query.value(0).toInt() : -1;
    }

    Options pointOptions() const {
        Options options = basicOptions();
        options.decimalSeparator = '.';
        return options;
    }

    static QStringList column(const QString& sql) {
        QStringList values;
        QSqlQuery query(DatabaseManager::instance().database());
        if (query.exec(sql)) {
            while (query.next()) values << query.value(0).toString();
        }
        return values;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        accountId = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        QVERIFY(accountId > 0);
    }
    void cleanup() { db.close(); }

    // --- Detecting the file's conventions ---

    void detectsDelimiter_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QChar>("expected");
        QTest::newRow("comma") << "Date,Amount,Payee\n2026-01-01,12.50,Shop\n" << QChar(',');
        QTest::newRow("semicolon with decimal commas") << "Datum;Betrag;Name\n01.01.2026;12,50;Shop\n02.01.2026;-3,10;Bakery\n" << QChar(';');
        QTest::newRow("tab") << "Date\tAmount\n2026-01-01\t12.50\n" << QChar('\t');
        QTest::newRow("pipe") << "Date|Amount|Payee\n2026-01-01|12.50|Shop\n" << QChar('|');
        QTest::newRow("quoted semicolons in a comma file") << "Date,Payee\n2026-01-01,\"A; B; C\"\n" << QChar(',');
        QTest::newRow("empty") << "" << QChar(',');
    }
    void detectsDelimiter() {
        QFETCH(QString, text);
        QFETCH(QChar, expected);
        QCOMPARE(detectDelimiter(text), expected);
    }

    void detectsDecimalSeparator() {
        QCOMPARE(detectDecimalSeparator({{"1.234,56"}, {"12,50"}}, 0, '.'), QChar(','));
        QCOMPARE(detectDecimalSeparator({{"12.50"}, {"-3.10"}}, 0, ','), QChar('.'));
        // US thousands separator must not count as a decimal comma (used to be detected as ',')
        QCOMPARE(detectDecimalSeparator({{"1,234.56"}, {"2,000.00"}}, 0, ','), QChar('.'));
        // Only "1,234"-style values: ambiguous, so the fallback decides
        QCOMPARE(detectDecimalSeparator({{"1,234"}}, 0, '.'), QChar('.'));
        QCOMPARE(detectDecimalSeparator({{"1,234"}}, 0, ','), QChar(','));
        // With an amount column, other columns (here a German date) don't vote
        QCOMPARE(detectDecimalSeparator({{"01.02.2026", "12,50"}, {"03.02.2026", "1,00"}}, 1, '.'), QChar(','));
    }

    void autoMapsEnglishAndGermanHeaders() {
        const Mapping english = autoMapColumns({"Date", "Amount", "Payee", "Memo", "Category"});
        QCOMPARE(english.value(FieldDate), 0);
        QCOMPARE(english.value(FieldAmount), 1);
        QCOMPARE(english.value(FieldPayee), 2);
        QCOMPARE(english.value(FieldDescription), 3);
        QCOMPARE(english.value(FieldCategory), 4);

        const Mapping german = autoMapColumns({" Buchungstag ", "Name Zahlungsbeteiligter", "Verwendungszweck", "BETRAG"});
        QCOMPARE(german.value(FieldDate), 0);
        QCOMPARE(german.value(FieldPayee), 1);
        QCOMPARE(german.value(FieldDescription), 2);
        QCOMPARE(german.value(FieldAmount), 3);

        // Used to have a double space typo
        QCOMPARE(autoMapColumns({"Trans. Date"}).value(FieldDate, -1), 0);
    }

    void autoMapUsesEachColumnOnce() {
        // "Type" used to be mapped to both Type and Category
        const Mapping mapping = autoMapColumns({"Date", "Amount", "Type"});
        QCOMPARE(mapping.value(FieldType, -1), 2);
        QVERIFY(!mapping.contains(FieldCategory));
    }

    // --- Reading single values ---

    void parsesDates_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QDate>("expected");
        QTest::newRow("iso") << "2026-01-15" << QDate(2026, 1, 15);
        QTest::newRow("german") << "15.01.2026" << QDate(2026, 1, 15);
        QTest::newRow("german short") << "5.1.2026" << QDate(2026, 1, 5);   // Format had a typo before
        QTest::newRow("day first wins") << "03/04/2026" << QDate(2026, 4, 3);
        QTest::newRow("us when day-first is impossible") << "01/15/2026" << QDate(2026, 1, 15);
        QTest::newRow("compact") << "20260115" << QDate(2026, 1, 15);
        QTest::newRow("month name") << "15 Jan 2026" << QDate(2026, 1, 15);
        QTest::newRow("us month name") << "Jan 15, 2026" << QDate(2026, 1, 15);
        QTest::newRow("surrounding spaces") << "  2026-01-15 " << QDate(2026, 1, 15);
        QTest::newRow("empty") << "" << QDate();
        QTest::newRow("garbage") << "not a date" << QDate();
    }
    void parsesDates() {
        QFETCH(QString, text);
        QFETCH(QDate, expected);
        QCOMPARE(parseDate(text), expected);
    }

    void parsesAmounts_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QChar>("separator");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<qint64>("expected");          // Cents
        QTest::newRow("german thousands") << "1.234,56" << QChar(',') << true << qint64(123456);
        QTest::newRow("us thousands") << "1,234.56" << QChar('.') << true << qint64(123456);
        QTest::newRow("currency and sign") << "-12,50 €" << QChar(',') << true << qint64(-1250);
        QTest::newRow("accounting negative") << "(12.50)" << QChar('.') << true << qint64(-1250);
        QTest::newRow("trailing minus") << "12,50-" << QChar(',') << true << qint64(-1250);
        QTest::newRow("explicit plus") << "+7.00" << QChar('.') << true << qint64(700);
        QTest::newRow("dollar") << "$ 1,000" << QChar('.') << true << qint64(100000);
        QTest::newRow("three decimals round up") << "12.345" << QChar('.') << true << qint64(1235);
        QTest::newRow("three decimals round down") << "12,344" << QChar(',') << true << qint64(1234);
        QTest::newRow("float-unfriendly value") << "0.29" << QChar('.') << true << qint64(29);  // 0.29 * 100 is 28.999... as double
        QTest::newRow("large") << "9.876.543,21" << QChar(',') << true << qint64(987654321);
        QTest::newRow("two separators left") << "1.2.3" << QChar(',') << true << qint64(12300);
        QTest::newRow("only a separator") << "." << QChar('.') << false << qint64(0);
        QTest::newRow("text") << "abc" << QChar('.') << false << qint64(0);
        QTest::newRow("empty") << "  " << QChar('.') << false << qint64(0);
    }
    void parsesAmounts() {
        QFETCH(QString, text);
        QFETCH(QChar, separator);
        QFETCH(bool, ok);
        QFETCH(qint64, expected);
        bool parsed = true;
        QCOMPARE(parseAmount(text, separator, &parsed), expected);
        QCOMPARE(parsed, ok);
    }

    void decidesTransactionType() {
        QCOMPARE(transactionType(-5, "", AmountInterpretation::SignBased), QString("expense"));
        QCOMPARE(transactionType(5, "", AmountInterpretation::SignBased), QString("income"));
        QCOMPARE(transactionType(-5, "", AmountInterpretation::SignBasedInverted), QString("income"));
        QCOMPARE(transactionType(-5, "", AmountInterpretation::AllIncome), QString("income"));
        QCOMPARE(transactionType(5, "", AmountInterpretation::AllExpense), QString("expense"));
        QCOMPARE(transactionType(5, "DEBIT", AmountInterpretation::ColumnBased), QString("expense"));
        QCOMPARE(transactionType(-5, "Credit", AmountInterpretation::ColumnBased), QString("income"));
        // Unknown text falls back to the sign
        QCOMPARE(transactionType(-5, "S", AmountInterpretation::ColumnBased), QString("expense"));
    }

    void buildsTransactionsAndExplainsFailures() {
        Options options = basicOptions();
        options.categoryIds.insert("groceries", 4);
        Mapping mapping = basicMapping();
        mapping.insert(FieldCategory, 4);

        const RowResult good = buildTransaction({"02.01.2026", "-12,50", "Rewe", "Weekly shop", "GROCERIES"}, 2, mapping, options);
        QVERIFY2(good.ok, qPrintable(good.error));
        QCOMPARE(good.transaction.getAmountCents(), Cents(1250));         // Stored positive, sign goes into the type
        QCOMPARE(good.transaction.getType(), QString("expense"));
        QCOMPARE(good.transaction.getAccountId(), accountId);
        QCOMPARE(good.transaction.getPayee(), QString("Rewe"));
        QCOMPARE(good.transaction.getCategoryId(), 4);        // Matched case-insensitively (used to be ignored)

        const RowResult unknownCategory = buildTransaction({"02.01.2026", "1,00", "", "", "Travel"}, 2, mapping, options);
        QCOMPARE(unknownCategory.transaction.getCategoryId(), 0);

        QCOMPARE(buildTransaction({"02.01.2026"}, 7, mapping, options).error,
                 QString("Row 7: Amount column not mapped or out of range"));
        QCOMPARE(buildTransaction({"yesterday", "1,00"}, 3, mapping, options).error,
                 QString("Row 3: Invalid date 'yesterday'"));
        // An empty amount used to be imported as 0
        QCOMPARE(buildTransaction({"02.01.2026", ""}, 4, mapping, options).error, QString("Row 4: Amount is missing"));
        QCOMPARE(buildTransaction({"02.01.2026", "0,00"}, 5, mapping, options).error, QString("Row 5: Amount is zero"));
        QCOMPARE(buildTransaction({"02.01.2026", "n/a"}, 6, mapping, options).error,
                 QString("Row 6: Failed to parse amount 'n/a'"));
    }

    // --- Writing to the database ---

    void importsAndSkipsDuplicates() {
        const QList<QStringList> rows = {
            {"02.01.2026", "-12,50", "Rewe", "Weekly shop"},
            {"03.01.2026", "2.850,00", "ACME", "Salary"},
        };
        Result first = run(rows, basicMapping(), basicOptions(), {});
        QCOMPARE(first.imported, 2);
        QCOMPARE(first.failed, 0);
        QCOMPARE(transactionCount(accountId), 2);

        // Same file again: everything is a duplicate
        Result second = run(rows, basicMapping(), basicOptions(), {});
        QCOMPARE(second.imported, 0);
        QCOMPARE(second.duplicates, 2);
        QCOMPARE(transactionCount(accountId), 2);

        // Unless duplicates are allowed
        RunOptions allowDuplicates;
        allowDuplicates.allowDuplicates = true;
        QCOMPARE(run(rows, basicMapping(), basicOptions(), allowDuplicates).imported, 2);
        QCOMPARE(transactionCount(accountId), 4);
    }

    void dryRunWritesNothing() {
        RunOptions dryRun;
        dryRun.dryRun = true;
        const Result result = run({{"02.01.2026", "-1,00", "A", ""}}, basicMapping(), basicOptions(), dryRun);
        QCOMPARE(result.imported, 1);
        QCOMPARE(transactionCount(accountId), 0);
    }

    // --- Balance check ---

    void mapsTheBalanceColumn() {
        const Mapping mapping = autoMapColumns({"Buchungstag", "Betrag", "Saldo nach Buchung"});
        QCOMPARE(mapping.value(FieldBalance, -1), 2);
    }

    void statedBalanceIsTheOneAfterTheNewestRow() {
        Mapping mapping = basicMapping();
        mapping.insert(FieldBalance, 4);
        // Oldest first: the last row of the newest day
        const QList<QStringList> oldestFirst = {
            {"01.10.2026", "-1,00", "A", "", "99,00"},
            {"17.10.2026", "-2,00", "B", "", "97,00"},
            {"17.10.2026", "-3,00", "C", "", "94,00"},
        };
        StatedBalance stated = statedBalance(oldestFirst, mapping, ',');
        QVERIFY(stated.found);
        QCOMPARE(stated.date, QDate(2026, 10, 17));
        QCOMPARE(stated.cents, Cents(9400));

        // Newest first: the first row
        const QList<QStringList> newestFirst = {oldestFirst[2], oldestFirst[1], oldestFirst[0]};
        stated = statedBalance(newestFirst, mapping, ',');
        QCOMPARE(stated.cents, Cents(9400));

        // Sheets of a workbook, each month newest first, the months oldest first: still the
        // first row of the newest day
        const QList<QStringList> monthsInOrder = {
            {"30.09.2026", "-1,00", "A", "", "120,00"},
            {"02.09.2026", "-1,00", "B", "", "121,00"},
            {"01.09.2026", "-1,00", "C", "", "122,00"},
            {"17.10.2026", "-3,00", "D", "", "94,00"},
            {"17.10.2026", "-2,00", "E", "", "97,00"},
            {"01.10.2026", "-1,00", "F", "", "99,00"},
        };
        stated = statedBalance(monthsInOrder, mapping, ',');
        QCOMPARE(stated.date, QDate(2026, 10, 17));
        QCOMPARE(stated.cents, Cents(9400));

        // Without the column (or values) there is nothing to compare with
        QVERIFY(!statedBalance(oldestFirst, basicMapping(), ',').found);
        QVERIFY(!statedBalance({{"01.10.2026", "-1,00", "A", "", ""}}, mapping, ',').found);
    }

    void dryRunReportsTheBalanceBeforeAndAfter() {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, accountId, "Old", 100000, QDate(2026, 9, 1), "", 0, "income")) > 0);
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, accountId, "Later", 5000, QDate(2026, 11, 1), "", 0, "expense")) > 0);
        RunOptions dryRun;
        dryRun.dryRun = true;
        const Result result = run({{"02.10.2026", "-12,50", "Rewe", ""}, {"17.10.2026", "200,00", "ACME", ""}},
                                  basicMapping(), basicOptions(), dryRun);
        QCOMPARE(result.checkDate, QDate(2026, 10, 17));             // The newest day in the file
        QCOMPARE(result.balanceBeforeCents, Cents(100000));           // November isn't counted
        QCOMPARE(result.importedNetCents, Cents(18750));
        QCOMPARE(result.balanceAfterCents, Cents(118750));
        QCOMPARE(transactionCount(accountId), 2);
    }

    void importAddsTheBalanceCorrection() {
        RunOptions correct;
        correct.correct = true;
        correct.correctTo = 18000;                                    // The bank says 180,00
        correct.correctionNote = "Import of test.csv";
        const Result result = run({{"02.10.2026", "-12,50", "Rewe", ""}, {"17.10.2026", "200,00", "ACME", ""}},
                                  basicMapping(), basicOptions(), correct);
        QCOMPARE(result.imported, 2);
        QCOMPARE(result.correctionCents, Cents(-750));
        QCOMPARE(transactionCount(accountId), 3);

        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT t.amount, t.type, t.date, c.name, t.description FROM transactions t "
                           "JOIN categories c ON c.id = t.category_id WHERE c.name = 'Balance correction'") && query.next());
        QCOMPARE(query.value(0).toLongLong(), 750);
        QCOMPARE(query.value(1).toString(), QString("expense"));
        QCOMPARE(query.value(2).toString(), QString("2026-10-17"));
        QCOMPARE(query.value(4).toString(), QString("Import of test.csv"));

        // Matching already: no correction
        const Result again = run({{"18.10.2026", "1,00", "X", ""}}, basicMapping(), basicOptions(),
                                 [] { RunOptions o; o.correct = true; o.correctTo = 18100; return o; }());
        QCOMPARE(again.correctionCents, Cents(0));
        QCOMPARE(transactionCount(accountId), 4);
    }

    void correctionInADryRunIsRolledBack() {
        RunOptions dryRun;
        dryRun.dryRun = true;
        dryRun.correct = true;
        dryRun.correctTo = 0;
        const Result result = run({{"02.10.2026", "-12,50", "Rewe", ""}}, basicMapping(), basicOptions(), dryRun);
        QCOMPARE(result.correctionCents, Cents(1250));
        QCOMPARE(transactionCount(accountId), 0);
    }

    // --- Categories named in the file ---

    void listsTheFilesCategories() {
        Mapping mapping = basicMapping();
        mapping.insert(FieldCategory, 4);
        const QList<QStringList> rows = {
            {"01.10.2026", "-10,00", "A", "", "Tanken"},
            {"02.10.2026", "-20,00", "B", "", "tanken "},
            {"03.10.2026", "100,00", "C", "", "Gehalt"},
            {"04.10.2026", "-5,00", "D", "", ""},
        };
        const QList<FileCategory> found = fileCategories(rows, mapping, basicOptions());
        QCOMPARE(found.size(), 2);
        QCOMPARE(found[0].name, QString("Tanken"));               // Same name in any case, most rows first
        QCOMPARE(found[0].rows, 2);
        QCOMPARE(found[0].likelyType(), QString("expense"));
        QCOMPARE(found[1].likelyType(), QString("income"));
        QVERIFY(fileCategories(rows, basicMapping(), basicOptions()).isEmpty());
    }

    void editedTransactionsAreStillRecognisedOnTheNextImport() {
        const QList<QStringList> rows = {{"01.10.2026", "-32,40", "REWE MARKT GMBH 4711", "Kartenzahlung"}};
        QCOMPARE(run(rows, basicMapping(), basicOptions(), {}).imported, 1);

        TransactionDAO dao;
        Transaction imported = dao.getTransactionsForAccount(accountId).first();
        imported.setPayee("Rewe");
        imported.setDescription("Wocheneinkauf");
        QVERIFY(dao.updateTransaction(imported));

        const Result again = run(rows, basicMapping(), basicOptions(), {});
        QCOMPARE(again.imported, 0);
        QCOMPARE(again.duplicates, 1);
    }

    void rulesRenamePayeesAndTheNextImportStillKnowsThem() {
        Rule rename;
        rename.newPayee = "Rewe";
        rename.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::Contains, "REWE MARKT")};
        rename.id = RuleDAO(1).addRule(rename);
        QVERIFY(rename.id > 0);
        Options options = basicOptions();
        options.rules = {rename};
        const QList<QStringList> rows = {{"01.10.2026", "-32,40", "REWE MARKT GMBH 4711", ""},
                                         {"02.10.2026", "-5,00", "Aldi", ""}};
        const Result result = run(rows, basicMapping(), options, {});
        QCOMPARE(result.imported, 2);
        QCOMPARE(result.renamedByRules, 1);

        Transaction renamed;
        for (const Transaction& transaction : TransactionDAO().getTransactionsForAccount(accountId)) {
            if (transaction.getAmountCents() == 3240) renamed = transaction;
        }
        QCOMPARE(renamed.getPayee(), QString("Rewe"));
        QCOMPARE(renamed.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));
        QCOMPARE(renamed.getRenamedByRuleId(), rename.id);

        const Result again = run(rows, basicMapping(), options, {});   // The same export once more
        QCOMPARE(again.imported, 0);
        QCOMPARE(again.duplicates, 2);
        QCOMPARE(again.renamedByRules, 0);               // Duplicates don't count
    }

    void rulesFillInCategoriesOrReplaceTheFilesWhenAllowed() {
        const auto categoryId = [](const QString& name) {
            for (const Category& category : CategoryDAO(1).getAllCategories()) {
                if (category.getName() == name) return category.getCategoryId();
            }
            return -1;
        };
        Rule rewe;
        rewe.categoryId = categoryId("Groceries");
        rewe.conditions = {RuleCondition(RuleCondition::Field::PayeeOrDescription, RuleCondition::Operator::Contains, "REWE")};
        Mapping mapping = basicMapping();
        mapping.insert(FieldCategory, 4);
        const QList<QStringList> rows = {{"01.10.2026", "-10,00", "REWE Markt", "", ""},
                                         {"02.10.2026", "-20,00", "REWE City", "", "Entertainment"},
                                         {"03.10.2026", "-30,00", "Aldi", "", ""}};
        const auto categoryOf = [](const QString& payee) {
            QSqlQuery query(DatabaseManager::instance().database());
            query.prepare("SELECT COALESCE(category_id, 0) FROM transactions WHERE payee = :payee");
            query.bindValue(":payee", payee);
            return query.exec() && query.next() ? query.value(0).toInt() : -1;
        };

        Options options = basicOptions();
        options.categoryIds.insert("entertainment", categoryId("Entertainment"));
        options.rules = {rewe};
        RunOptions dryRun;
        dryRun.dryRun = true;
        Result result = run(rows, mapping, options, dryRun);
        QCOMPARE(result.categorisedByRules, 1);          // Only the row without a category
        options.rulesOverrideFileCategories = true;
        result = run(rows, mapping, options, {});
        QCOMPARE(result.categorisedByRules, 2);
        QCOMPARE(categoryOf("REWE Markt"), categoryId("Groceries"));
        QCOMPARE(categoryOf("REWE City"), categoryId("Groceries"));
        QCOMPARE(categoryOf("Aldi"), 0);                 // No rule matches
    }

    void createsNewCategoriesOnImportOnly() {
        Mapping mapping = basicMapping();
        mapping.insert(FieldCategory, 4);
        const QList<QStringList> rows = {{"01.10.2026", "-10,00", "Aral", "", "Tanken"}, {"02.10.2026", "-20,00", "Shell", "", "TANKEN"}};
        Options options = basicOptions();
        options.categoriesToCreate.insert("tanken", {"Tanken", "expense"});
        auto categoryCount = [] {
            QSqlQuery query(DatabaseManager::instance().database());
            return query.exec("SELECT COUNT(*) FROM categories WHERE name = 'Tanken'") && query.next() ? query.value(0).toInt() : -1;
        };

        RunOptions dryRun;
        dryRun.dryRun = true;
        QCOMPARE(run(rows, mapping, options, dryRun).categoriesCreated, 1);
        QCOMPARE(categoryCount(), 0);                             // Rolled back

        const Result result = run(rows, mapping, options, {});
        QCOMPARE(result.imported, 2);
        QCOMPARE(categoryCount(), 1);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT COUNT(*) FROM transactions t JOIN categories c ON c.id = t.category_id WHERE c.name = 'Tanken'") && query.next());
        QCOMPARE(query.value(0).toInt(), 2);
    }

    void unreadableRowsAbortUnlessPartialImport() {
        const QList<QStringList> rows = {
            {"02.01.2026", "-1,00", "Good", ""},
            {"not a date", "-2,00", "Bad", ""},
            {"04.01.2026", "-3,00", "Also good", ""},
        };

        // Partial import off: one bad row stops everything (used to import the good rows anyway)
        const Result strict = run(rows, basicMapping(), basicOptions(), {});
        QCOMPARE(strict.imported, 0);
        QCOMPARE(strict.failed, 3);
        QVERIFY(strict.errors.first().startsWith("Import aborted"));
        QCOMPARE(transactionCount(accountId), 0);

        RunOptions partial;
        partial.allowPartialImport = true;
        const Result lenient = run(rows, basicMapping(), basicOptions(), partial);
        QCOMPARE(lenient.imported, 2);
        QCOMPARE(lenient.failed, 1);
        QCOMPARE(lenient.errors, QStringList({"Row 3: Invalid date 'not a date'"}));
        QCOMPARE(transactionCount(accountId), 2);

        // Rows from a workbook's sheets say where they are
        RunOptions labelled;
        labelled.allowPartialImport = true;
        labelled.dryRun = true;
        labelled.rowLabels = {"Januar, row 2", "Februar, row 5", "Februar, row 6"};
        QCOMPARE(run(rows, basicMapping(), basicOptions(), labelled).errors,
                 QStringList({"Februar, row 5: Invalid date 'not a date'"}));
    }

    void databaseErrorsRollBack() {
        // An account that doesn't exist makes every insert fail (foreign key)
        Options options = basicOptions();
        options.accountId = 9999;
        const QList<QStringList> rows = {{"02.01.2026", "-1,00", "A", ""}, {"03.01.2026", "-2,00", "B", ""}};

        const Result strict = run(rows, basicMapping(), options, {});
        QCOMPARE(strict.imported, 0);
        QCOMPARE(strict.failed, 2);
        QVERIFY(strict.errors.first().startsWith("Import aborted"));

        RunOptions partial;
        partial.allowPartialImport = true;
        const Result lenient = run(rows, basicMapping(), options, partial);
        QCOMPARE(lenient.imported, 0);
        QCOMPARE(lenient.failed, 2);
        QCOMPARE(transactionCount(9999), 0);

        // The database is still usable afterwards
        QCOMPARE(run(rows, basicMapping(), basicOptions(), {}).imported, 2);
    }

    // --- Hostile input: what banks, spreadsheets and people really produce. A wrong value must
    // never be saved silently; refusing one is fine, since the import then says which row it was. ---

    void textWithNumbersInItIsNotAnAmount_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("an invoice number") << "Rechnung 4711";
        QTest::newRow("scientific notation") << "1.5E+3";
        QTest::newRow("a date in the wrong column") << "2026-01-02";
        QTest::newRow("two numbers") << "3 x 5,00";
        QTest::newRow("an iban") << "DE89 3704 0044 0532 0130 00";
        QTest::newRow("an excel error") << "#DIV/0!";
        QTest::newRow("a short word") << "Nr 4711";
    }
    void textWithNumbersInItIsNotAnAmount() {
        QFETCH(QString, text);
        bool ok = true;
        const Cents cents = parseAmount(text, ',', &ok);
        QVERIFY2(!ok, qPrintable(QString("read as %1").arg(Money::toPlainString(cents))));
    }

    void realWorldAmountsStillWork_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QChar>("separator");
        QTest::addColumn<qint64>("cents");
        QTest::newRow("unicode minus") << QString::fromUtf8("−12,50") << QChar(',') << qint64(-1250);
        QTest::newRow("en dash") << QString::fromUtf8("– 12,50") << QChar(',') << qint64(-1250);
        QTest::newRow("no-break space thousands") << QString("1") + QChar(0x00A0) + "234,56" << QChar(',') << qint64(123456);
        QTest::newRow("thin space thousands") << QString("1") + QChar(0x202F) + "234,56" << QChar(',') << qint64(123456);
        QTest::newRow("swiss thousands") << "1'234.56" << QChar('.') << qint64(123456);
        QTest::newRow("currency code after") << "12,50 EUR" << QChar(',') << qint64(1250);
        QTest::newRow("currency code before") << "EUR -12,50" << QChar(',') << qint64(-1250);
        QTest::newRow("symbol before the sign") << QString::fromUtf8("€-12.50") << QChar('.') << qint64(-1250);
        QTest::newRow("spelled out") << "12,50 Euro" << QChar(',') << qint64(1250);
        QTest::newRow("negative zero") << "-0,00" << QChar(',') << qint64(0);
    }
    void realWorldAmountsStillWork() {
        QFETCH(QString, text);
        QFETCH(QChar, separator);
        QFETCH(qint64, cents);
        bool ok = false;
        QCOMPARE(parseAmount(text, separator, &ok), Cents(cents));
        QVERIFY(ok);
    }

    // --- Dates ---

    void twoDigitYearsAreThisCentury_data() {
        QTest::addColumn<QString>("locale");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QDate>("expected");
        // Some bank exports write dates like this
        QTest::newRow("german") << "de_DE" << "15.01.26" << QDate(2026, 1, 15);
        QTest::newRow("german, c locale") << "C" << "15.01.26" << QDate(2026, 1, 15);
        QTest::newRow("us") << "en_US" << "1/15/26" << QDate(2026, 1, 15);
    }
    void twoDigitYearsAreThisCentury() {
        QFETCH(QString, locale);
        QFETCH(QString, text);
        QFETCH(QDate, expected);
        const auto restoreLocale = qScopeGuard([previous = QLocale()] { QLocale::setDefault(previous); });
        QLocale::setDefault(QLocale(locale));
        QCOMPARE(parseDate(text), expected);
    }

    void impossibleDatesAreRefused_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("30 february") << "30.02.2026";
        QTest::newRow("29 february, no leap year") << "2026-02-29";
        QTest::newRow("31 april") << "31/04/2026";
        QTest::newRow("month 13 both ways") << "13/13/2026";
        QTest::newRow("day zero") << "00.01.2026";
        QTest::newRow("a number") << "12345";
        QTest::newRow("an amount") << "12,50";
    }
    void impossibleDatesAreRefused() {
        QFETCH(QString, text);
        const QDate date = parseDate(text);
        QVERIFY2(!date.isValid(), qPrintable(date.toString(Qt::ISODate)));
    }

    void delimiterSurvivesUntidyFiles_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QChar>("expected");
        QTest::newRow("a blank line at the end") << "Datum;Betrag;Name\n01.01.2026;12,50;Shop\n\n" << QChar(';');
        QTest::newRow("a blank line in between") << "Datum;Betrag;Name\n\n01.01.2026;12,50;Shop\n" << QChar(';');
        QTest::newRow("a title line above the table") << "Umsaetze Girokonto\nDatum;Betrag;Name\n01.01.2026;12,50;Shop\n" << QChar(';');
        QTest::newRow("a title with a comma") << "Umsaetze, Girokonto\nDatum;Betrag;Name\n01.01.2026;12,50;Shop\n" << QChar(';');
        QTest::newRow("decimal commas in every row") << "Titel\nDatum;Betrag\n01.01.2026;12,50\n02.01.2026;3,10\n" << QChar(';');
        QTest::newRow("one row that's cut short") << "Date,Amount,Payee\n2026-01-01,12.50,Shop\n2026-01-02\n2026-01-03,1.00,Cafe\n" << QChar(',');
        QTest::newRow("no delimiter at all") << "Name\nShop\n" << QChar(',');
    }
    void delimiterSurvivesUntidyFiles() {
        QFETCH(QString, text);
        QFETCH(QChar, expected);
        QCOMPARE(detectDelimiter(text), expected);
    }

    void sollAndHabenMeanSomething_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");
        // autoMapColumns maps a "Soll/Haben" column, and such files have unsigned amounts
        QTest::newRow("S") << "S" << "expense";
        QTest::newRow("Soll") << "Soll" << "expense";
        QTest::newRow("H") << "H" << "income";
        QTest::newRow("Haben") << "Haben" << "income";
    }
    void sollAndHabenMeanSomething() {
        QFETCH(QString, text);
        QFETCH(QString, expected);
        QCOMPARE(transactionType(1250, text, AmountInterpretation::ColumnBased), expected);
    }

    void twoEqualCoffeesOnOneDayAreBothKept() {
        const QList<QStringList> rows = {{"2026-01-02", "-3.20", "Cafe", "Coffee"}, {"2026-01-02", "-3.20", "Cafe", "Coffee"}};
        const Result first = run(rows, basicMapping(), pointOptions(), {});
        QCOMPARE(first.imported, 2);
        const Result second = run(rows, basicMapping(), pointOptions(), {});
        QCOMPARE(second.imported, 0);
        QCOMPARE(second.duplicates, 2);
    }

    void aHugeFileIsStillCheckedForDuplicates() {
        // More rows than SQLite takes variables in one query (32766 by default). If the check
        // fails, every row looks new and the whole file is imported twice.
        Transaction known(-1, accountId, "Known", 100, QDate(2026, 1, 2), "", 0, "expense");
        TransactionDAO dao;
        QVERIFY(dao.addTransaction(known) > 0);
        QList<QString> hashes;
        for (int i = 0; i < 33000; ++i) hashes.append(QString("not there %1").arg(i));
        hashes.append(known.calculateHash());
        QCOMPARE(dao.getExistingHashes(hashes), QList<QString>({known.calculateHash()}));
    }

    void textIsDecodedByWhatItIs() {
        QCOMPARE(decodeText(QByteArray("M\xC3\xBCller")), QString::fromUtf8("Müller"));
        QCOMPARE(decodeText(QByteArray("\xEF\xBB\xBFM\xC3\xBCller")), QString::fromUtf8("Müller"));
        QCOMPARE(decodeText(QByteArray("M\xFCller")), QString::fromUtf8("Müller"));
        QCOMPARE(decodeText(QByteArray("\x80\xA0" "5 \x96 x")), QString::fromUtf8("€\u00A05 – x"));
        QCOMPARE(decodeText(QByteArray("\xFF\xFEM\0\xFC\0", 6)), QString::fromUtf8("Mü"));
        QCOMPARE(decodeText(QByteArray()), QString());
    }

    void yearsAlreadyInFullStayAsTheyAre() {
        QCOMPARE(parseDate("15.01.1999"), QDate(1999, 1, 15));
        // A two-digit year that would be far in the future is last century's
        QCOMPARE(parseDate("15.01.99"), QDate(1999, 1, 15));
    }

    void filesFromOlderBankSoftwareKeepTheirUmlauts_data() {
        QTest::addColumn<QByteArray>("bytes");
        const QByteArray utf8 = QString::fromUtf8("Datum;Betrag;Empfänger\n02.01.2026;-1,00;Müller\n").toUtf8();
        QTest::newRow("utf-8") << utf8;
        QTest::newRow("utf-8 with a byte order mark") << QByteArray("\xEF\xBB\xBF") + utf8;
        // What older bank software may export
        QTest::newRow("latin-1") << QByteArray("Datum;Betrag;Empf\xE4nger\n02.01.2026;-1,00;M\xFCller\n");
    }
    void filesFromOlderBankSoftwareKeepTheirUmlauts() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        const QString path = dir.filePath("umsaetze.csv");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();

        ImportManagerDialog dialog(1);
        dialog.findChild<QComboBox*>("accountComboBox")->setCurrentIndex(0);
        dialog.loadFile(path);
        auto* next = dialog.findChild<QPushButton*>("nextButton");
        next->click();                                       // Columns
        next->click();                                       // Review
        QCOMPARE(dialog.currentStep(), ImportManagerDialog::Step::Review);
        QTimer closer;                                       // The "Import complete" message waits for a click
        connect(&closer, &QTimer::timeout, this, [] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) message->accept();
        });
        closer.start(20);
        dialog.findChild<QPushButton*>("importButton")->click();
        QCOMPARE(column("SELECT payee FROM transactions"), QStringList({QString::fromUtf8("Müller")}));
    }

    void hostileTextIsStoredAsIs() {
        const QString payee = QString::fromUtf8("Robert'); DROP TABLE transactions;-- \"%1\" 🎃");
        const QString description = "line one\nline two\t%2 100%";
        const QList<QStringList> rows = {{"2026-01-02", "-1.00", payee, description}};
        QCOMPARE(run(rows, basicMapping(), pointOptions(), {}).imported, 1);
        QCOMPARE(column("SELECT payee FROM transactions"), QStringList({payee}));
        QCOMPARE(column("SELECT description FROM transactions"), QStringList({description}));
        QCOMPARE(run(rows, basicMapping(), pointOptions(), {}).duplicates, 1);
    }

    void shortRowsFailWithoutTakingTheOthersDown() {
        const QList<QStringList> rows = {{"2026-01-02", "-1.00", "Good", ""}, {"2026-01-03"}, {}, {"2026-01-04", "-2.00"}};
        RunOptions runOptions;
        runOptions.allowPartialImport = true;
        const Result result = run(rows, basicMapping(), pointOptions(), runOptions);
        QCOMPARE(result.imported, 2);
        QCOMPARE(result.failed, 2);
        QCOMPARE(column("SELECT SUM(amount) FROM transactions"), QStringList({"300"}));
    }
};

QTEST_MAIN(TestCsvImport)
#include "test_csvimport.moc"
