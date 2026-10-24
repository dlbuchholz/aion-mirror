#ifndef RULESSETTINGS_H
#define RULESSETTINGS_H

#include <QHash>
#include <QList>
#include <QWidget>
#include <functional>

#include "category.h"
#include "rule.h"
#include "transaction.h"

class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;
class ConditionEditor;

// Settings → Rules: the user's rules in the order they're tried, with an inline editor.
// Changes are saved right away.
class RulesSettings : public QWidget {
    Q_OBJECT

public:
    explicit RulesSettings(QWidget* parent = nullptr);

    void setUserId(int userId);                      // Shows and edits that user's rules
    void reload();                                   // Rules, categories, accounts and transactions changed

    // "Payee or description contains REWE and amount between 40,00 € and 120,00 €"
    QString describe(const Rule& rule) const;

    // Asks before deleting (a message box; tests replace it)
    std::function<bool(const QString& title, const QString& text)> confirm;

signals:
    void rulesChanged();

private:
    void rebuildList();
    void openEditor(int ruleId);                     // 0 = new rule
    void closeEditor();
    void addConditionRow(const RuleCondition& condition);
    void save();
    void remove();
    void move(int ruleId, int offset);               // -1 = up, +1 = down
    void updateMatchCount();
    std::optional<Rule> editedRule(QString* error) const;
    int matchCount(const Rule& rule) const;

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
    QComboBox* categoryCombo = nullptr;
    QLabel* matchCountLabel = nullptr;
    QLabel* errorLabel = nullptr;
    QPushButton* deleteButton = nullptr;
};

#endif // RULESSETTINGS_H
