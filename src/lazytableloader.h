#ifndef LAZYTABLELOADER_H
#define LAZYTABLELOADER_H

#include <QObject>
#include <QTableWidget>
#include <QScrollBar>
#include <QSet>
#include <QList>
#include <functional>
#include "transaction.h"

class LazyTableLoader :  public QObject {
    Q_OBJECT

public:
    explicit LazyTableLoader(QTableWidget* tableWidget, QObject* parent = nullptr);

    // Configuration
    void setBatchSize(int size);
    void setVisibleWindowSize(int size);
    void setScrollDistanceThreshold(int threshold);

    // Data management
    void setAllData(const QList<Transaction>& transactions);
    const QList<Transaction>& getAllData() const;
    int getTotalCount() const;
    int getLoadedCount() const;

    // Callbacks for row insertion
    using RowInsertCallback = std::function<void(int row, const Transaction& transaction, bool isShadowed)>;
    void setRowInsertCallback(RowInsertCallback callback);

    // Pinning (for selected/modified rows)
    void setPinnedTransactionIds(const QSet<int>& ids);
    void addPinnedTransactionId(int id);
    void removePinnedTransactionId(int id);
    void clearPinnedTransactionIds();

    // Loading control
    void loadInitialBatch();
    void loadMoreTransactions();
    void loadEarlierTransactions();
    void unloadDistantTransactions();
    void clear();

    // Enable/disable
    void setEnabled(bool enabled);
    bool isEnabled() const;

signals:
    void loadingStateChanged(bool isLoading);
    void countChanged(int visible, int total);

private slots:
    void onScrollBarValueChanged(int value);

private:
    bool shouldKeepTransactionLoaded(int transactionId) const;
    void connectScrollBar();
    void disconnectScrollBar();

    QTableWidget* tableWidget;
    QList<Transaction> allTransactions;
    RowInsertCallback rowInsertCallback;

    // Lazy loading state
    QSet<int> loadedTransactionIds;
    int batchSize;
    int visibleWindowSize;
    bool isLoadingMore;
    QSet<int> pinnedTransactionIds;

    // Scroll tracking
    int lastUnloadScrollPosition;
    int scrollDistanceThreshold;

    // Enable/disable
    bool enabled;
};

#endif // LAZYTABLELOADER_H
