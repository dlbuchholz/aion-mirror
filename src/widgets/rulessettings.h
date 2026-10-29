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
    // Opens the editor filled with draft: a new rule, or with an ID that rule with the draft's changes.
    // The match count then also says how many of the examples it matches.
    void openDraft(const Rule& draft, const QList<Transaction>& examples = {});

    // "Payee or description contains REWE and amount between 40,00 € and 120,00 €", whether or not it has a name
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
    // 0 = new rule; filled with draft if given (instead of the saved rule)
    void openEditor(int ruleId, const Rule* draft = nullptr, const QList<Transaction>& examples = {});
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
    RuleMatchIndex matchIndex;                       // The user's transactions, for the match counts
    QHash<int, int> matchCounts;                     // By rule ID; counted when a rule is first listed or saved
    int editedId = -1;                               // -1 = editor closed, 0 = new rule
    QList<Transaction> draftExamples;                // The transactions a draft was made from

    QPushButton* addButton = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QLabel* emptyLabel = nullptr;

    QFrame* editor = nullptr;
    QLineEdit* nameEdit = nullptr;
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
