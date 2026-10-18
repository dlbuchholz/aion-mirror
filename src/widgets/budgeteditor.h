#ifndef BUDGETEDITOR_H
#define BUDGETEDITOR_H

#include <QDate>
#include <QFrame>
#include "budget.h"
#include "category.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QMenu;
class QPushButton;
class QRadioButton;

// Side panel for adding or changing one category's limit in a month. Like the transaction
// editor it does not touch the database: the budget page persists what the signals carry.
class BudgetEditor : public QFrame {
    Q_OBJECT

public:
    explicit BudgetEditor(QWidget* parent = nullptr);

    void applySettings();                            // Decimal separator for the limit field

    // categories: the ones that can get a new budget in this month
    void newBudget(const QDate& month, const QList<Category>& categories, int categoryId = 0, Cents suggestedLimit = 0);
    void editBudget(const QDate& month, const MonthBudget& budget);
    int editedCategoryId() const;                    // 0 while adding a new budget

    void showError(const QString& message);

signals:
    void saveRequested(int categoryId, Cents limitCents, bool everyMonth);
    void removeRequested(int categoryId, bool fromThisMonthOn);
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setMonth(const QDate& month);
    void requestSave();

    QDate month;
    int categoryId = 0;                              // Existing budget being edited

    QLabel* titleLabel = nullptr;
    QComboBox* categoryCombo = nullptr;
    QLabel* categoryLabel = nullptr;                 // Shown instead of the combo when editing
    QDoubleSpinBox* limitEdit = nullptr;
    QRadioButton* everyMonthButton = nullptr;
    QRadioButton* onlyThisMonthButton = nullptr;
    QLabel* hintLabel = nullptr;
    QLabel* errorLabel = nullptr;
    QPushButton* removeButton = nullptr;
    QMenu* removeMenu = nullptr;                     // Regular limits: from this month on, or only this month
};

#endif // BUDGETEDITOR_H
