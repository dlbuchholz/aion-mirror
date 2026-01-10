#include "lazytableloader.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <algorithm>
#include <QDebug>

LazyTableLoader::LazyTableLoader(QTableWidget* tableWidget, QObject* parent)
    : QObject(parent),
    tableWidget(tableWidget),
    loadedTransactionCount(0),
    batchSize(50),
    visibleWindowSize(200),
    isLoadingMore(false),
    lastUnloadScrollPosition(0),
    scrollDistanceThreshold(100),
    enabled(true)
{
    if (tableWidget) {
        connectScrollBar();
    }
}

void LazyTableLoader::setBatchSize(int size) {
    batchSize = size;
}

void LazyTableLoader:: setVisibleWindowSize(int size) {
    visibleWindowSize = size;
}

void LazyTableLoader::setScrollDistanceThreshold(int threshold) {
    scrollDistanceThreshold = threshold;
}

void LazyTableLoader::setAllData(const QList<Transaction>& transactions) {
    allTransactions = transactions;
    loadedTransactionCount = 0;
}

const QList<Transaction>& LazyTableLoader::getAllData() const {
    return allTransactions;
}

int LazyTableLoader::getTotalCount() const {
    return allTransactions.size();
}

int LazyTableLoader::getLoadedCount() const {
    return tableWidget ? tableWidget->rowCount() : 0;
}

void LazyTableLoader::setRowInsertCallback(RowInsertCallback callback) {
    rowInsertCallback = callback;
}

void LazyTableLoader::setPinnedTransactionIds(const QSet<int>& ids) {
    pinnedTransactionIds = ids;
}

void LazyTableLoader::addPinnedTransactionId(int id) {
    pinnedTransactionIds.insert(id);
}

void LazyTableLoader::removePinnedTransactionId(int id) {
    pinnedTransactionIds.remove(id);
}

void LazyTableLoader::clearPinnedTransactionIds() {
    pinnedTransactionIds.clear();
}

void LazyTableLoader:: setEnabled(bool en) {
    if (enabled == en) return;

    enabled = en;

    if (enabled) {
        connectScrollBar();
    } else {
        disconnectScrollBar();
    }
}

bool LazyTableLoader::isEnabled() const {
    return enabled;
}

void LazyTableLoader:: connectScrollBar() {
    if (tableWidget && tableWidget->verticalScrollBar()) {
        connect(tableWidget->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &LazyTableLoader::onScrollBarValueChanged);
    }
}

void LazyTableLoader::disconnectScrollBar() {
    if (tableWidget && tableWidget->verticalScrollBar()) {
        disconnect(tableWidget->verticalScrollBar(), &QScrollBar::valueChanged,
                   this, &LazyTableLoader::onScrollBarValueChanged);
    }
}

void LazyTableLoader::loadInitialBatch() {
    if (!enabled || ! tableWidget) return;

    loadedTransactionCount = 0;
    loadMoreTransactions();
}

void LazyTableLoader::loadMoreTransactions() {
    if (!enabled || !tableWidget || isLoadingMore) return;

    if (loadedTransactionCount >= allTransactions.size()) {
        qDebug() << "All transactions already loaded";
        return;
    }

    isLoadingMore = true;
    emit loadingStateChanged(true);

    QScrollBar* scrollBar = tableWidget->verticalScrollBar();
    int oldScrollValue = scrollBar ?  scrollBar->value() : 0;

    int startIndex = loadedTransactionCount;
    int remainingCount = allTransactions.size() - startIndex;
    int countToLoad = qMin(batchSize, remainingCount);

    qDebug() << "Loading batch:" << startIndex << "to" << (startIndex + countToLoad - 1)
             << "of" << allTransactions.size() << "total transactions";

    tableWidget->blockSignals(true);

    for (int i = 0; i < countToLoad; ++i) {
        int transactionIndex = startIndex + i;
        if (transactionIndex >= allTransactions.size()) break;

        const Transaction& transaction = allTransactions[transactionIndex];
        bool isShadowed = (transaction.getAccountId() <= 0);
        int row = tableWidget->rowCount();

        if (rowInsertCallback) {
            rowInsertCallback(row, transaction, isShadowed);
        }
    }

    tableWidget->blockSignals(false);

    loadedTransactionCount += countToLoad;

    if (scrollBar) {
        scrollBar->setValue(oldScrollValue);
    }

    emit countChanged(getLoadedCount(), getTotalCount());
    emit loadingStateChanged(false);
    isLoadingMore = false;
}

void LazyTableLoader::loadEarlierTransactions() {
    if (!enabled || !tableWidget || isLoadingMore || tableWidget->rowCount() == 0) return;

    QString firstId = tableWidget->verticalHeaderItem(0)->text();
    int firstTransactionId = firstId.toInt();

    int firstIndex = -1;
    for (int i = 0; i < allTransactions.size(); ++i) {
        if (allTransactions[i].getTransactionId() == firstTransactionId) {
            firstIndex = i;
            break;
        }
    }

    if (firstIndex <= 0) {
        qDebug() << "Already at start of transactions";
        return;
    }

    isLoadingMore = true;
    emit loadingStateChanged(true);

    int firstVisibleRow = tableWidget->rowAt(0);
    if (firstVisibleRow < 0) firstVisibleRow = 0;

    int countToLoad = qMin(batchSize, firstIndex);
    int startIndex = firstIndex - countToLoad;

    qDebug() << "Loading earlier batch:" << startIndex << "to" << (firstIndex - 1);

    tableWidget->blockSignals(true);

    for (int i = countToLoad - 1; i >= 0; --i) {
        int transactionIndex = startIndex + i;
        const Transaction& transaction = allTransactions[transactionIndex];
        bool isShadowed = (transaction.getAccountId() <= 0);

        if (rowInsertCallback) {
            rowInsertCallback(0, transaction, isShadowed);
        }
    }

    tableWidget->blockSignals(false);

    int newTargetRow = firstVisibleRow + countToLoad;
    tableWidget->scrollToItem(
        tableWidget->verticalHeaderItem(newTargetRow),
        QAbstractItemView::PositionAtTop
        );

    qDebug() << "Adjusted scroll from row" << firstVisibleRow << "to" << newTargetRow;

    emit countChanged(getLoadedCount(), getTotalCount());
    emit loadingStateChanged(false);
    isLoadingMore = false;
}

void LazyTableLoader::unloadDistantTransactions() {
    if (!enabled || !tableWidget) return;

    int unloadThreshold = visibleWindowSize + batchSize;

    if (tableWidget->rowCount() <= unloadThreshold) {
        return;
    }

    QScrollBar* scrollBar = tableWidget->verticalScrollBar();
    int firstVisibleRow = tableWidget->rowAt(0);
    if (firstVisibleRow < 0) firstVisibleRow = 0;

    QString visibleTransactionId = "";
    if (firstVisibleRow < tableWidget->rowCount()) {
        QTableWidgetItem* headerItem = tableWidget->verticalHeaderItem(firstVisibleRow);
        if (headerItem) {
            visibleTransactionId = headerItem->text();
        }
    }

    int lastVisibleRow = tableWidget->rowAt(tableWidget->viewport()->height());
    if (lastVisibleRow < 0) lastVisibleRow = tableWidget->rowCount() - 1;

    int bufferSize = visibleWindowSize / 2;
    int keepStartRow = qMax(0, firstVisibleRow - bufferSize);
    int keepEndRow = qMin(tableWidget->rowCount() - 1, lastVisibleRow + bufferSize);

    QList<int> rowsToRemove;
    int farThreshold = bufferSize / 2;

    for (int row = 0; row < keepStartRow - farThreshold; ++row) {
        QString id = tableWidget->verticalHeaderItem(row)->text();
        int transactionId = id.toInt();

        if (! shouldKeepTransactionLoaded(transactionId)) {
            rowsToRemove.append(row);
        }
    }

    for (int row = tableWidget->rowCount() - 1; row > keepEndRow + farThreshold; --row) {
        QString id = tableWidget->verticalHeaderItem(row)->text();
        int transactionId = id.toInt();

        if (!shouldKeepTransactionLoaded(transactionId)) {
            rowsToRemove. append(row);
        }
    }

    if (rowsToRemove.size() < batchSize / 2) {
        return;
    }

    std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

    tableWidget->blockSignals(true);

    for (int row : rowsToRemove) {
        tableWidget->removeRow(row);
        if (row < loadedTransactionCount) {
            loadedTransactionCount--;
        }
    }

    tableWidget->blockSignals(false);

    if (! visibleTransactionId.isEmpty()) {
        for (int row = 0; row < tableWidget->rowCount(); ++row) {
            QTableWidgetItem* headerItem = tableWidget->verticalHeaderItem(row);
            if (headerItem && headerItem->text() == visibleTransactionId) {
                tableWidget->scrollToItem(headerItem, QAbstractItemView::PositionAtTop);
                break;
            }
        }
    }

    emit countChanged(getLoadedCount(), getTotalCount());
}

void LazyTableLoader::clear() {
    allTransactions.clear();
    loadedTransactionCount = 0;
    pinnedTransactionIds.clear();
    lastUnloadScrollPosition = 0;

    if (tableWidget) {
        tableWidget->blockSignals(true);
        tableWidget->setRowCount(0);
        tableWidget->blockSignals(false);
    }
}

bool LazyTableLoader::shouldKeepTransactionLoaded(int transactionId) const {
    if (pinnedTransactionIds.contains(transactionId)) {
        return true;
    }

    if (transactionId < 0) {
        return true;
    }

    return false;
}

void LazyTableLoader::onScrollBarValueChanged(int value) {
    if (!enabled || !tableWidget) return;

    QScrollBar* scrollBar = tableWidget->verticalScrollBar();
    if (! scrollBar) return;

    // Load more when near bottom
    int bottomThreshold = scrollBar->maximum() * 0.8;
    if (value >= bottomThreshold && ! isLoadingMore) {
        loadMoreTransactions();
    }

    // Load earlier when near top
    int topThreshold = scrollBar->maximum() * 0.2;
    if (value <= topThreshold && ! isLoadingMore && tableWidget->rowCount() < allTransactions.size()) {
        loadEarlierTransactions();
    }

    // Unload distant transactions
    int scrollDistance = qAbs(value - lastUnloadScrollPosition);
    int rowHeight = tableWidget->rowHeight(0);
    if (rowHeight <= 0) rowHeight = 30;

    int rowsScrolled = scrollDistance / rowHeight;

    if (rowsScrolled >= scrollDistanceThreshold && tableWidget->rowCount() > visibleWindowSize) {
        unloadDistantTransactions();
        lastUnloadScrollPosition = value;
    }
}
