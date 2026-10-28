#include "transactionlistmodel.h"
#include "category.h"

#include <QSet>
#include <algorithm>

TransactionListModel::TransactionListModel(QObject* parent)
    : QAbstractListModel(parent) {}

void TransactionListModel::setTransactions(const QList<Transaction>& transactions,
                                           const QHash<int, QString>& newAccountNames,
                                           const QHash<int, QString>& newCategoryNames) {
    allTransactions = transactions;
    accountNames = newAccountNames;
    categoryNames = newCategoryNames;

    // Newest first; for the same day, the most recently added first
    std::sort(allTransactions.begin(), allTransactions.end(), [](const Transaction& a, const Transaction& b) {
        if (a.getDate() != b.getDate()) {
            return a.getDate() > b.getDate();
        }
        return a.getTransactionId() > b.getTransactionId();
    });
    rebuild();
}

void TransactionListModel::setFilter(const TransactionFilter& filter) {
    currentFilter = filter;
    rebuild();
}

void TransactionListModel::setCategoryColors(const QHash<int, QString>& colors) {
    categoryColors = colors;
    if (!rows.isEmpty()) emit dataChanged(index(0), index(int(rows.size()) - 1));
}

void TransactionListModel::setCategoryTypes(const QHash<int, QString>& types) {
    categoryTypes = types;
    rebuild();                                       // The totals depend on them
}

const TransactionFilter& TransactionListModel::filter() const {
    return currentFilter;
}

TransactionListModel::Totals TransactionListModel::totals() const {
    return currentTotals;
}

std::optional<Transaction> TransactionListModel::transaction(int transactionId) const {
    for (const Transaction& transaction : allTransactions) {
        if (transaction.getTransactionId() == transactionId) {
            return transaction;
        }
    }
    return std::nullopt;
}

QModelIndex TransactionListModel::indexForTransaction(int transactionId) const {
    for (int row = 0; row < rows.size(); ++row) {
        if (rows[row].kind == Kind::Transaction
            && allTransactions[rows[row].transactionIndex].getTransactionId() == transactionId) {
            return index(row);
        }
    }
    return QModelIndex();
}

QStringList TransactionListModel::payees() const {
    QSet<QString> seen;
    QStringList result;
    for (const Transaction& transaction : allTransactions) {
        const QString payee = transaction.getPayee().trimmed();
        if (!payee.isEmpty() && !seen.contains(payee)) {
            seen.insert(payee);
            result.append(payee);
        }
    }
    return result;
}

int TransactionListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : rows.size();
}

QVariant TransactionListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows.size()) {
        return QVariant();
    }

    const Row& row = rows[index.row()];
    if (role == KindRole) {
        return static_cast<int>(row.kind);
    }
    if (role == DateRole) {
        return row.date;
    }
    if (row.kind == Kind::DayHeader) {
        switch (role) {
        case CollapsedRole:  return collapsedDays.contains(row.date);
        case DayCountRole:   return row.dayTransactions.size();
        case DayNetRole:     return row.dayNet;
        case Qt::ToolTipRole:
            return QString("Click to fold or unfold · Ctrl+click to select this day · Shift+click to select up to this day");
        default:             return QVariant();
        }
    }

    const Transaction& transaction = allTransactions[row.transactionIndex];
    switch (role) {
    case Qt::DisplayRole:
    case PayeeRole:
        return transaction.getPayee();
    case TransactionIdRole:
        return transaction.getTransactionId();
    case DescriptionRole:
        return transaction.getDescription();
    case CategoryRole:
        return categoryNames.value(transaction.getCategoryId());
    case CategoryColorRole:
        return categoryColors.value(transaction.getCategoryId());
    case AccountRole:
        return accountNames.value(transaction.getAccountId());
    case SignedAmountRole:
        return transaction.getType() == "income" ? transaction.getAmountCents() : -transaction.getAmountCents();
    default:
        return QVariant();
    }
}

Qt::ItemFlags TransactionListModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    if (rows[index.row()].kind == Kind::DayHeader) {
        return Qt::ItemIsEnabled;                    // Clickable (to fold), but not selectable
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool TransactionListModel::matches(const Transaction& transaction) const {
    if (currentFilter.from.isValid() && transaction.getDate() < currentFilter.from) {
        return false;
    }
    if (currentFilter.to.isValid() && transaction.getDate() > currentFilter.to) {
        return false;
    }
    if (currentFilter.categoryId != TransactionFilter::ANY_CATEGORY) {
        const int categoryId = transaction.getCategoryId() > 0 ? transaction.getCategoryId() : TransactionFilter::NO_CATEGORY;
        if (categoryId != currentFilter.categoryId) {
            return false;
        }
    }

    const QString search = currentFilter.search.trimmed();
    if (search.isEmpty()) {
        return true;
    }
    const QStringList haystack = {
        transaction.getPayee(),
        transaction.getDescription(),
        categoryNames.value(transaction.getCategoryId()),
        accountNames.value(transaction.getAccountId())
    };
    for (const QString& text : haystack) {
        if (text.contains(search, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void TransactionListModel::rebuild() {
    beginResetModel();
    rows.clear();
    currentTotals = Totals();

    QDate currentDay;
    int headerRow = -1;
    for (int i = 0; i < allTransactions.size(); ++i) {
        const Transaction& transaction = allTransactions[i];
        if (!matches(transaction)) {
            continue;
        }
        if (transaction.getDate() != currentDay) {
            currentDay = transaction.getDate();
            rows.append({Kind::DayHeader, currentDay, -1, {}, 0});
            headerRow = rows.size() - 1;
        }

        const bool income = transaction.getType() == "income";
        Row& header = rows[headerRow];
        header.dayTransactions.append(i);
        header.dayNet += income ? transaction.getAmountCents() : -transaction.getAmountCents();

        // Folded days keep their header (with count and net) but hide the transactions
        if (!collapsedDays.contains(currentDay)) {
            rows.append({Kind::Transaction, currentDay, i, {}, 0});
        }

        currentTotals.count++;
        // The sums are income and spending, like the Dashboard's: balance adjustments and transfers stay out
        if (isUncountedCategory(categoryNames.value(transaction.getCategoryId()))) {
            continue;
        }
        if (isRefund(transaction.getType(), categoryTypes.value(transaction.getCategoryId()))) {
            currentTotals.expenses -= transaction.getAmountCents();
        } else if (income) {
            currentTotals.income += transaction.getAmountCents();
        } else {
            currentTotals.expenses += transaction.getAmountCents();
        }
    }
    endResetModel();
}

void TransactionListModel::toggleDay(const QDate& date) {
    int headerRow = -1;
    for (int row = 0; row < rows.size(); ++row) {
        if (rows[row].kind == Kind::DayHeader && rows[row].date == date) {
            headerRow = row;
            break;
        }
    }
    if (headerRow < 0) {
        return;
    }

    // Insert or remove only the day's rows, so selection and scroll position elsewhere stay put
    const QList<int> members = rows[headerRow].dayTransactions;
    if (collapsedDays.contains(date)) {
        collapsedDays.remove(date);
        beginInsertRows(QModelIndex(), headerRow + 1, headerRow + members.size());
        for (int i = 0; i < members.size(); ++i) {
            rows.insert(headerRow + 1 + i, {Kind::Transaction, date, members[i], {}, 0});
        }
        endInsertRows();
    } else {
        collapsedDays.insert(date);
        beginRemoveRows(QModelIndex(), headerRow + 1, headerRow + members.size());
        rows.remove(headerRow + 1, members.size());
        endRemoveRows();
    }
    const QModelIndex header = index(headerRow);
    emit dataChanged(header, header);
}

bool TransactionListModel::isDayCollapsed(const QDate& date) const {
    return collapsedDays.contains(date);
}
