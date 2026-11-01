#include <QtTest>

#include "csvparser.h"

class TestCsvParser : public QObject {
    Q_OBJECT

private slots:
    void parsesQuotedFieldsAndEscapedQuotes() {
        QString error;
        const auto rows = CsvParser::parse(
            "Date,Amount,Payee,Description\r\n"
            "2026-08-27,12.50,\"Shop, Berlin\",\"He said \"\"hello\"\"\"\r\n",
            ',', &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[1][2], QString("Shop, Berlin"));
        QCOMPARE(rows[1][3], QString("He said \"hello\""));
    }

    void parsesMultilineAndEmptyFields() {
        QString error;
        const auto rows = CsvParser::parse("a;b;c\n1;;\"two\nlines\"", ';', &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[1], QStringList({"1", "", "two\nlines"}));
    }

    void rejectsUnterminatedQuote() {
        QString error;
        const auto rows = CsvParser::parse("a,b\n1,\"broken", ',', &error);
        QVERIFY(rows.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    // --- Hostile input: what banks, spreadsheets and people really produce. A wrong value must
    // never be saved silently; refusing one is fine, since the import then says which row it was. ---

    void parserKeepsItsPromisesOnBadQuotes() {
        QString error;
        QCOMPARE(CsvParser::parse("\"a\"b,c\n", ',', &error), QList<QStringList>({{"ab", "c"}}));
        QCOMPARE(CsvParser::parse("a\"b\"c,d\n", ',', &error), QList<QStringList>({{"a\"b\"c", "d"}}));
        QCOMPARE(CsvParser::parse("a,\"\"\n", ',', &error), QList<QStringList>({{"a", ""}}));
        QVERIFY(error.isEmpty());
        QCOMPARE(CsvParser::parse("a\rb\r\nc\n", ',', &error), QList<QStringList>({{"a"}, {"b"}, {"c"}}));
        // An odd number of quotes must never swallow the rest of the file quietly
        const auto rows = CsvParser::parse("a,\"b\nc,d\ne,f\n", ',', &error);
        QVERIFY(rows.isEmpty());
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestCsvParser)
#include "test_csvparser.moc"
