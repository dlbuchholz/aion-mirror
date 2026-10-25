#ifndef RULESSETTINGS_H
#define RULESSETTINGS_H

#include <QHash>
#include <QList>
#include <QWidget>
#include <functional>

#include "category.h"
#include "rule.h"
#include "transaction.h"

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class ConditionEditor;
class RenamedPayeesDialog;

// Settings → Rules: the user's rules in the order they're tried, with an inline editor.
// Changes are saved right away.
class RulesSettings : public QWidget {
    Q_OBJECT

public:
    explicit RulesSettings(QWidget* parent = nullptr);

    void setUserId(int userId);                      // Shows and edits that user's rules
    void reload();                                   // Rules, categories, accounts and transactions changed
    void createRule(const Rule& draft);              // Opens the editor for a new rule filled with draft

    // "Payee or description contains REWE and amount between 40,00 € and 120,00 €"
    QString describe(const Rule& rule) const;

    // Asks before deleting (a message box; tests replace it)
    std::function<bool(const QString& title, const QString& text)> confirm;
    // Shows the dialog offering to restore or update a rule's names (exec; tests replace it)
    std::function<void(RenamedPayeesDialog& dialog)> reviewRenamedPayees;

signals:
    void rulesChanged();
    void transactionsChanged();                      // Rules were applied to existing transactions

private:
    void rebuildList();
    void openEditor(int ruleId, const Rule* draft = nullptr);   // 0 = new rule, filled with draft if given
    void closeEditor();
    void addConditionRow(const RuleCondition& condition);
    void save();
    void remove();
    void move(int ruleId, int offset);               // -1 = up, +1 = down
    void updateMatchCount();
    std::optional<Rule> editedRule(QString* error) const;
    int matchCount(const Rule& rule) const;
    // The rule's rename was removed or the rule deleted (newPayee empty), or its name changed:
    // offers to do the same to the transactions it renamed
    void offerToUpdateNames(const QList<Transaction>& renamed, const Rule& before, const QString& newPayee);

    int userId = -1;
    QList<Rule> rules;
    QList<Category> categories;
    QHash<int, QString> accountNames;
    QList<Transaction> transactions;                 // The user's, for the match counts
    int editedId = -1;                               // -1 = editor closed, 0 = new rule

    QPushButton* addButton = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QLabel* emptyLabel = nullptr;

    QFrame* editor = nullptr;
    QComboBox* matchCombo = nullptr;
    QVBoxLayout* conditionsLayout = nullptr;
    QList<ConditionEditor*> conditionEditors;
    QCheckBox* setsCategoryCheck = nullptr;
    QComboBox* categoryCombo = nullptr;
    QCheckBox* renamesCheck = nullptr;
    QLineEdit* newPayeeEdit = nullptr;
    QLabel* matchCountLabel = nullptr;
    QLabel* errorLabel = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // RULESSETTINGS_H
