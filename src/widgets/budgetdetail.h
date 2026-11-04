#ifndef BUDGETDETAIL_H
#define BUDGETDETAIL_H

#include <QAbstractButton>
#include <QDate>
#include <QFrame>
#include "budget.h"

// A row that lights up under the mouse and with focus; Enter clicks it
class HoverRow : public QAbstractButton {
    Q_OBJECT

public:
    explicit HoverRow(QWidget* parent = nullptr);
    void setStrongHighlight(bool strong);            // For rows on a background that is already a hover colour

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    bool strongHighlight = false;
};

// A budget's month: the spending rises with every transaction (a dot each), next to the limit,
// an even pace from nothing to the limit, and today. Hovering a dot names its transaction;
// clicking it asks for that transaction.
class BudgetMonthChart : public QWidget {
    Q_OBJECT

public:
    BudgetMonthChart(const QDate& month, Cents limitCents, const QList<BudgetTransaction>& transactions,
                     const QDate& today, QWidget* parent = nullptr);

    QPointF dotCenter(int index) const;
    int transactionAt(const QPointF& position) const;          // Index of the dot there, -1 for none
    QString dotText(int index) const;                          // "18 Nov · Rewe · 20,50 €"

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void transactionClicked(int transactionId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plot() const;
    double xFor(double day) const;
    double yFor(Cents cents) const;
    double dayOf(int index) const;                             // Middle of the transaction's day
    double lastDay() const;                                    // Where the line ends

    QDate month;
    Cents limitCents;
    QList<BudgetTransaction> transactions;
    QList<Cents> totals;                                       // Spent after each transaction
    QDate today;
    Cents lowest = 0;
    Cents highest = 1;
    int hovered = -1;
};

// A budget row unfolded: its month as a chart, how it's going (per day, the pace, last month,
// what's left per day), the latest transactions, and ways on to all of them and the editor.
class BudgetDetail : public QFrame {
    Q_OBJECT

public:
    BudgetDetail(const MonthBudget& budget, const QDate& month, const QList<BudgetTransaction>& transactions,
                 Cents lastMonthSpent, Cents lastMonthLimit, const QDate& today, QWidget* parent = nullptr);

    static constexpr int ShownTransactions = 4;

signals:
    void transactionRequested(int transactionId);
    void transactionsRequested();                              // All of them, on the Transactions page
    void editRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
};

#endif // BUDGETDETAIL_H
