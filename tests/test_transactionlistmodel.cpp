#include <QtTest>
#include "transactionlistmodel.h"

class TestTransactionListModel : public QObject {
    Q_OBJECT

    using Kind = TransactionListModel::Kind;

    static QList<Transaction> sample() {
        return {
            Transaction(1, 10, "Rewe", 4000, QDate(2026, 10, 5), "Weekly shop", 4, "expense"),
            Transaction(2, 10, "ACME", 200000, QDate(2026, 10, 15), "Salary", 2, "income"),
            Transaction(3, 20, "Kino", 1250, QDate(2026, 10, 15), "", 0, "expense"),
            Transaction(4, 20, "Rewe", 2000, QDate(2026, 9, 30), "", 4, "expense"),
        };
    }

    static void load(TransactionListModel& model) {
        model.setTransactions(sample(), {{10, "Girokonto"}, {20, "Kreditkarte"}},
                              {{2, "Salary"}, {4, "Groceries"}});
    }

    static Kind kindAt(const TransactionListModel& model, int row) {
        return static_cast<Kind>(model.index(row).data(TransactionListModel::KindRole).toInt());
    }

private slots:
    void groupsByDayNewestFirst() {
        TransactionListModel model;
        load(model);

        // 3 days -> 3 headers + 4 transactions
        QCOMPARE(model.rowCount(), 7);
        QCOMPARE(kindAt(model, 0), Kind::DayHeader);
        QCOMPARE(model.index(0).data(TransactionListModel::DateRole).toDate(), QDate(2026, 10, 15));
        // Same day: higher ID (added later) first
        QCOMPARE(model.index(1).data(TransactionListModel::TransactionIdRole).toInt(), 3);
        QCOMPARE(model.index(2).data(TransactionListModel::TransactionIdRole).toInt(), 2);
        QCOMPARE(kindAt(model, 5), Kind::DayHeader);
        QCOMPARE(model.index(6).data(TransactionListModel::TransactionIdRole).toInt(), 4);

        QVERIFY(!(model.flags(model.index(0)) & Qt::ItemIsSelectable));
        QVERIFY(model.flags(model.index(1)) & Qt::ItemIsSelectable);
    }

    void exposesNamesAndSignedAmounts() {
        TransactionListModel model;
        load(model);
        const QModelIndex salary = model.indexForTransaction(2);
        QCOMPARE(salary.data(TransactionListModel::AccountRole).toString(), QString("Girokonto"));
        QCOMPARE(salary.data(TransactionListModel::CategoryRole).toString(), QString("Salary"));
        QCOMPARE(salary.data(TransactionListModel::SignedAmountRole).toLongLong(), 200000);
        QCOMPARE(model.indexForTransaction(3).data(TransactionListModel::SignedAmountRole).toLongLong(), -1250);
        QCOMPARE(model.indexForTransaction(3).data(TransactionListModel::CategoryRole).toString(), QString());
    }

    void totalsFollowFilter() {
        TransactionListModel model;
        load(model);
        QCOMPARE(model.totals().count, 4);
        QCOMPARE(model.totals().income, Cents(200000));
        QCOMPARE(model.totals().expenses, Cents(7250));
        QCOMPARE(model.totals().net(), Cents(192750));

        TransactionFilter october;
        october.from = QDate(2026, 10, 1);
        october.to = QDate(2026, 10, 31);
        model.setFilter(october);
        QCOMPARE(model.totals().count, 3);
        QCOMPARE(model.totals().expenses, Cents(5250));
        QVERIFY(!model.indexForTransaction(4).isValid());
    }

    void totalsLeaveOutTransfersAndBalanceAdjustments() {
        // Like the Dashboard: the sums are income and spending; the count is every shown transaction
        TransactionListModel model;
        QList<Transaction> transactions = sample();
        transactions << Transaction(5, 10, "To savings", 50000, QDate(2026, 10, 6), "", 7, "expense")
                     << Transaction(6, 20, "From Girokonto", 50000, QDate(2026, 10, 6), "", 7, "income")
                     << Transaction(7, 10, "Start", 100000, QDate(2026, 9, 1), "", 8, "income")
                     << Transaction(8, 10, "Correction", 300, QDate(2026, 10, 7), "", 9, "expense");
        model.setTransactions(transactions, {{10, "Girokonto"}, {20, "Kreditkarte"}},
                              {{2, "Salary"}, {4, "Groceries"}, {7, "Transfer"}, {8, "Initial balance"},
                               {9, "Balance correction"}});
        QCOMPARE(model.totals().count, 8);
        QCOMPARE(model.totals().income, Cents(200000));
        QCOMPARE(model.totals().expenses, Cents(7250));
        QCOMPARE(model.indexForTransaction(5).data(TransactionListModel::SignedAmountRole).toLongLong(), -50000);
    }

    void refundsLowerTheSpendingInsteadOfAddingIncome() {
        TransactionListModel model;
        QList<Transaction> transactions = sample();
        transactions << Transaction(9, 10, "Rewe", 1500, QDate(2026, 10, 8), "Pfand", 4, "income")    // Groceries refund
                     << Transaction(10, 10, "Unknown", 700, QDate(2026, 10, 8), "", 0, "income");     // No category
        model.setCategoryTypes({{2, "income"}, {4, "expense"}});
        model.setTransactions(transactions, {{10, "Girokonto"}, {20, "Kreditkarte"}}, {{2, "Salary"}, {4, "Groceries"}});
        QCOMPARE(model.totals().income, Cents(200000 + 700));
        QCOMPARE(model.totals().expenses, Cents(7250 - 1500));
    }

    void searchMatchesPayeeCategoryAndAccount() {
        TransactionListModel model;
        load(model);

        TransactionFilter filter;
        filter.search = "rewe";
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 2);

        filter.search = "groceries";
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 2);

        filter.search = "kredit";
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 2);

        filter.search = "weekly";
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 1);
    }

    void categoryFilterIncludesNoCategory() {
        TransactionListModel model;
        load(model);

        TransactionFilter filter;
        filter.categoryId = 4;
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 2);

        filter.categoryId = TransactionFilter::NO_CATEGORY;
        model.setFilter(filter);
        QCOMPARE(model.totals().count, 1);
        QVERIFY(model.indexForTransaction(3).isValid());
    }

    void collapsedDaysKeepHeaderWithSummary() {
        TransactionListModel model;
        load(model);
        const QDate day(2026, 10, 15);

        model.toggleDay(day);
        QVERIFY(model.isDayCollapsed(day));
        // The 15th loses its 2 transaction rows; header stays with count and net
        QCOMPARE(model.rowCount(), 5);
        QCOMPARE(kindAt(model, 0), Kind::DayHeader);
        QCOMPARE(model.index(0).data(TransactionListModel::CollapsedRole).toBool(), true);
        QCOMPARE(model.index(0).data(TransactionListModel::DayCountRole).toInt(), 2);
        QCOMPARE(model.index(0).data(TransactionListModel::DayNetRole).toLongLong(), 198750);
        QCOMPARE(kindAt(model, 1), Kind::DayHeader);
        // Totals still cover folded days
        QCOMPARE(model.totals().count, 4);

        // Folding survives a reload, unfolding restores the rows
        load(model);
        QCOMPARE(model.rowCount(), 5);
        model.toggleDay(day);
        QCOMPARE(model.rowCount(), 7);
        QCOMPARE(model.index(1).data(TransactionListModel::TransactionIdRole).toInt(), 3);
    }

    void payeesAreDistinct() {
        TransactionListModel model;
        load(model);
        QStringList payees = model.payees();
        payees.sort();
        QCOMPARE(payees, QStringList({"ACME", "Kino", "Rewe"}));
    }
};

QTEST_MAIN(TestTransactionListModel)
#include "test_transactionlistmodel.moc"
