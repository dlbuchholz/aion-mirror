#ifndef BUDGETPAGE_H
#define BUDGETPAGE_H

#include <QDate>
#include <QWidget>
#include <functional>
#include "budget.h"

class BudgetDetail;
class BudgetEditor;
struct BudgetReport;
class QAbstractButton;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// One month's category budgets: totals, a row per budget with its progress (one at a time unfolds
// to its month in detail), the spending that has no budget yet, and an editor panel for adding
// and changing limits.
class BudgetPage : public QWidget {
    Q_OBJECT

public:
    explicit BudgetPage(QWidget* parent = nullptr);

    // The day the tiles' pace goes by (tests replace it)
    std::function<QDate()> today = [] { return QDate::currentDate(); };

    void setUserId(int userId);
    void showMonth(const QDate& month);              // Any day in the month
    QDate month() const;
    void reload();                                   // Re-read budgets and spending
    void applySettings();                            // Number formats changed

signals:
    void budgetsChanged();
    // A category's transactions (0 = no category) in the shown month, from first to last day
    void transactionsRequested(int categoryId, const QDate& from, const QDate& to);
    void transactionRequested(int transactionId);    // One of them, from an unfolded budget
    void budgetReportRequested(const QDate& month);  // Budget vs. actual for the shown month

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void addBudget(int categoryId = 0, Cents suggestedLimit = 0);
    void editBudget(int categoryId);
    void onSaveRequested(int categoryId, Cents limitCents, bool everyMonth);
    void onRemoveRequested(int categoryId, bool fromThisMonthOn);
    void closeEditor();
    void showTransactions(int categoryId);           // In the shown month

private:
    QFrame* createTile(const QString& caption, QLabel** valueLabel, QLabel** noteLabel = nullptr);
    void updateHeader();
    // The tiles' second lines: the pace and what's left per day this month, how it ended in one that's over
    void updateNotes(const BudgetReport& report, const QDate& day);
    void updateIcons();                              // Re-tint icons after a theme change
    void clearRows();
    void highlightRow(int categoryId);               // The budget open in the editor (0 = none)
    void setExpanded(int categoryId);                // Unfolds that budget's row (0 = none)

    int userId = -1;
    QDate shownMonth;                                // First day of the shown month
    QList<MonthBudget> budgets;
    QList<UnbudgetedSpending> unbudgeted;
    int expandedCategoryId = 0;                      // Stays unfolded across reloads, and months it has a budget in

    QLabel* monthLabel = nullptr;
    QToolButton* previousButton = nullptr;
    QToolButton* nextButton = nullptr;
    QPushButton* thisMonthButton = nullptr;
    QPushButton* addButton = nullptr;
    QToolButton* reportLink = nullptr;               // To Budget vs. actual
    QLabel* budgetedLabel = nullptr;
    QLabel* spentLabel = nullptr;
    QLabel* spentNote = nullptr;
    QLabel* leftLabel = nullptr;
    QLabel* leftNote = nullptr;
    QLabel* leftCaption = nullptr;
    QVBoxLayout* rowsLayout = nullptr;
    QList<QAbstractButton*> budgetRows;
    QLabel* emptyLabel = nullptr;
    QLabel* unbudgetedTitle = nullptr;
    QVBoxLayout* unbudgetedLayout = nullptr;
    BudgetDetail* detail = nullptr;                  // Below the unfolded row
    BudgetEditor* editor = nullptr;
};

#endif // BUDGETPAGE_H
