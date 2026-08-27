#include <QtTest>
#include <QTableWidget>

#include "lazytableloader.h"

class TestLazyTableLoader : public QObject {
    Q_OBJECT

private slots:
    void loadingAfterUnloadDoesNotDuplicateIds() {
        QTableWidget table;
        table.setColumnCount(1);
        table.resize(400, 200);
        table.show();

        LazyTableLoader loader(&table);
        loader.setBatchSize(20);
        loader.setVisibleWindowSize(20);
        loader.setScrollDistanceThreshold(0);
        loader.setRowInsertCallback([&table](int row, const Transaction& transaction, bool) {
            table.insertRow(row);
            table.setVerticalHeaderItem(row,
                                        new QTableWidgetItem(QString::number(transaction.getTransactionId())));
            table.setItem(row, 0, new QTableWidgetItem(transaction.getDescription()));
        });

        QList<Transaction> transactions;
        for (int id = 1; id <= 120; ++id) {
            transactions.append(Transaction(id, 1, "", id, QDate(2026, 1, 1),
                                            QString::number(id), 0, "expense"));
        }
        loader.setAllData(transactions);
        loader.loadInitialBatch();
        for (int i = 0; i < 5; ++i) loader.loadMoreTransactions();
        QCOMPARE(table.rowCount(), 120);

        table.scrollToBottom();
        QCoreApplication::processEvents();
        loader.unloadDistantTransactions();
        loader.loadMoreTransactions();

        QSet<int> ids;
        for (int row = 0; row < table.rowCount(); ++row) {
            const int id = table.verticalHeaderItem(row)->text().toInt();
            QVERIFY2(!ids.contains(id), "A transaction ID was loaded twice");
            ids.insert(id);
        }
    }
};

QTEST_MAIN(TestLazyTableLoader)
#include "test_lazytableloader.moc"
