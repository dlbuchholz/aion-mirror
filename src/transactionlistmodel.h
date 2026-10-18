#ifndef TRANSACTIONLISTMODEL_H
#define TRANSACTIONLISTMODEL_H

#include <QAbstractListModel>
#include <QDate>
#include <QHash>
#include <QList>
#include <QSet>
#include <optional>
#include "transaction.h"

// Which transactions the list shows
struct TransactionFilter {
    static constexpr int ANY_CATEGORY = -1;
    static constexpr int NO_CATEGORY = 0;

    QString search;                                  // Matches payee, description, category or account
    QDate from;                                      // Inclusive; invalid = no lower bound
    QDate to;                                        // Inclusive; invalid = no upper bound
    int categoryId = ANY_CATEGORY;
};

// Flat list of transactions, newest first, with a header row before each day
class TransactionListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum class Kind {
        DayHeader,
        Transaction
    };

    enum Role {
        KindRole = Qt::UserRole + 1,
        DateRole,
        TransactionIdRole,
        PayeeRole,
        DescriptionRole,
        CategoryRole,
        AccountRole,
        SignedAmountRole,                            // Income positive, expense negative
        CollapsedRole,                               // Day headers: the day is folded
        DayCountRole,                                // Day headers: transactions on that day
        DayNetRole                                   // Day headers: income minus expenses of that day
    };

    struct Totals {
        int count = 0;
        double income = 0.0;
        double expenses = 0.0;                       // Positive sum of expense amounts
        double net() const { return income - expenses; }
    };

    explicit TransactionListModel(QObject* parent = nullptr);

    void setTransactions(const QList<Transaction>& transactions,
                         const QHash<int, QString>& accountNames,
                         const QHash<int, QString>& categoryNames);
    void setFilter(const TransactionFilter& filter);
    const TransactionFilter& filter() const;

    Totals totals() const;                           // Of the transactions passing the filter
    std::optional<Transaction> transaction(int transactionId) const;
    QModelIndex indexForTransaction(int transactionId) const;
    QStringList payees() const;                      // Distinct payees of all transactions, for completion

    void toggleDay(const QDate& date);               // Fold or unfold a day; kept across reloads
    bool isDayCollapsed(const QDate& date) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct Row {
        Kind kind;
        QDate date;
        int transactionIndex = -1;                   // Index into allTransactions for transaction rows
        QList<int> dayTransactions;                  // Day headers: indexes of the day's transactions
        double dayNet = 0.0;                         // Day headers: income minus expenses
    };

    void rebuild();
    bool matches(const Transaction& transaction) const;

    QList<Transaction> allTransactions;
    QHash<int, QString> accountNames;
    QHash<int, QString> categoryNames;
    TransactionFilter currentFilter;
    QSet<QDate> collapsedDays;
    QList<Row> rows;
    Totals currentTotals;
};

#endif // TRANSACTIONLISTMODEL_H
