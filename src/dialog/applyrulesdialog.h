#ifndef APPLYRULESDIALOG_H
#define APPLYRULESDIALOG_H

#include <QDialog>
#include <QHash>

#include "category.h"
#include "rule.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

// Shows which of the user's transactions the rules would give another category, and changes
// them only when confirmed
class ApplyRulesDialog : public QDialog {
    Q_OBJECT

public:
    explicit ApplyRulesDialog(int userId, QWidget* parent = nullptr);

    const QList<RuleChange>& changes() const { return pending; }

private:
    void updatePreview();
    void apply();

    QList<Rule> rules;
    QList<Transaction> transactions;
    QList<int> protectedCategories;                  // Balance adjustments keep their category
    QHash<int, Category> categoryById;
    QList<RuleChange> pending;

    QCheckBox* onlyUncategorisedCheck = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QLabel* summaryLabel = nullptr;
    QPushButton* applyButton = nullptr;
};

#endif // APPLYRULESDIALOG_H
