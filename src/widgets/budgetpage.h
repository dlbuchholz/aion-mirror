#ifndef BUDGETPAGE_H
#define BUDGETPAGE_H

#include <QDate>
#include <QWidget>
#include "budget.h"

class BudgetEditor;
class QAbstractButton;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// One month's category budgets: totals, a row per budget with its progress, the spending
// that has no budget yet, and an editor panel for adding and changing limits.
class BudgetPage : public QWidget {
    Q_OBJECT

public:
    explicit BudgetPage(QWidget* parent = nullptr);

    void setUserId(int userId);
    void showMonth(const QDate& month);              // Any day in the month
    QDate month() const;
    void reload();                                   // Re-read budgets and spending
    void applySettings();                            // Number formats changed

signals:
    void budgetsChanged();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void addBudget(int categoryId = 0, Cents suggestedLimit = 0);
    void editBudget(int categoryId);
    void onSaveRequested(int categoryId, Cents limitCents, bool everyMonth);
    void onRemoveRequested(int categoryId, bool fromThisMonthOn);
    void closeEditor();

private:
    QFrame* createTile(const QString& caption, QLabel** valueLabel);
    void updateHeader();
    void updateIcons();                              // Re-tint icons after a theme change
    void clearRows();
    void highlightRow(int categoryId);               // The budget open in the editor (0 = none)

    int userId = -1;
    QDate shownMonth;                                // First day of the shown month
    QList<MonthBudget> budgets;
    QList<UnbudgetedSpending> unbudgeted;

    QLabel* monthLabel = nullptr;
    QToolButton* previousButton = nullptr;
    QToolButton* nextButton = nullptr;
    QPushButton* thisMonthButton = nullptr;
    QPushButton* addButton = nullptr;
    QLabel* budgetedLabel = nullptr;
    QLabel* spentLabel = nullptr;
    QLabel* leftLabel = nullptr;
    QLabel* leftCaption = nullptr;
    QVBoxLayout* rowsLayout = nullptr;
    QList<QAbstractButton*> budgetRows;
    QLabel* emptyLabel = nullptr;
    QLabel* unbudgetedTitle = nullptr;
    QVBoxLayout* unbudgetedLayout = nullptr;
    BudgetEditor* editor = nullptr;
};

#endif // BUDGETPAGE_H
