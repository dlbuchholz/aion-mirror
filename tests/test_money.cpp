#include <QtTest>
#include <QCryptographicHash>

#include "money.h"
#include "transaction.h"

class TestMoney : public QObject {
    Q_OBJECT

private slots:
    void plainStringIsStable() {
        QCOMPARE(Money::toPlainString(1250), QString("12.50"));
        QCOMPARE(Money::toPlainString(-310), QString("-3.10"));
        QCOMPARE(Money::toPlainString(5), QString("0.05"));
        QCOMPARE(Money::toPlainString(-5), QString("-0.05"));
        QCOMPARE(Money::toPlainString(0), QString("0.00"));
        QCOMPARE(Money::toPlainString(123456789012), QString("1234567890.12"));
    }

    void parsesPlainDecimals_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("valid");
        QTest::addColumn<qint64>("cents");
        QTest::newRow("whole") << "12" << true << qint64(1200);
        QTest::newRow("one decimal") << "12.5" << true << qint64(1250);
        QTest::newRow("two decimals") << "12.34" << true << qint64(1234);
        QTest::newRow("round half up") << "0.125" << true << qint64(13);
        QTest::newRow("round down") << "0.124" << true << qint64(12);
        QTest::newRow("leading separator") << ".5" << true << qint64(50);
        QTest::newRow("trailing separator") << "5." << true << qint64(500);
        QTest::newRow("empty") << "" << false << qint64(0);
        QTest::newRow("separator only") << "." << false << qint64(0);
        QTest::newRow("two separators") << "1.2.3" << false << qint64(0);
        QTest::newRow("letters") << "12a" << false << qint64(0);
        QTest::newRow("sign is not parsed here") << "-1" << false << qint64(0);
        QTest::newRow("too long") << "1234567890123456" << false << qint64(0);
    }
    void parsesPlainDecimals() {
        QFETCH(QString, text);
        QFETCH(bool, valid);
        QFETCH(qint64, cents);
        const std::optional<Cents> parsed = Money::parsePlain(text);
        QCOMPARE(parsed.has_value(), valid);
        if (valid) QCOMPARE(*parsed, cents);
    }

    void convertsSpinBoxValuesExactly() {
        // 0.29 * 100 is 28.999... in floating point; rounding must still give 29
        QCOMPARE(Money::fromDouble(0.29), Cents(29));
        QCOMPARE(Money::fromDouble(-1234.56), Cents(-123456));
        QCOMPARE(Money::fromDouble(0.005), Cents(1));
        QCOMPARE(Money::toDouble(1250), 12.5);

        // The classic float sum is exact in cents
        QCOMPARE(Money::fromDouble(0.1) + Money::fromDouble(0.2), Money::fromDouble(0.3));
    }

    void transactionHashMatchesTheOldFormat() {
        // Hashes of transactions imported before the switch to cents must stay the same,
        // otherwise duplicate detection wouldn't recognise them anymore
        const Transaction transaction(-1, 7, "Rewe", 1250, QDate(2026, 1, 2), "Weekly shop", 0, "expense");
        const QString oldSource = "7|2026-01-02|12.50|expense|rewe|weekly shop";
        const QString expected = QCryptographicHash::hash(oldSource.toUtf8(), QCryptographicHash::Sha256).toHex();
        QCOMPARE(transaction.calculateHash(), expected);
    }
};

QTEST_MAIN(TestMoney)
#include "test_money.moc"
