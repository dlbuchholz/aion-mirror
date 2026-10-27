#ifndef RULE_H
#define RULE_H

#include <QHash>
#include <QList>
#include <QString>
#include <optional>

#include "transaction.h"

// One condition of a rule, like "payee contains REWE" or "amount between 40 and 120"
struct RuleCondition {
    enum class Field { Payee, Description, PayeeOrDescription, Amount, Type, Account };
    enum class Operator { Contains, Is, StartsWith, Between };

    RuleCondition() = default;
    RuleCondition(Field field, Operator op, const QString& value, const QString& value2 = QString())
        : field(field), op(op), value(value), value2(value2) {}

    Field field = Field::Payee;
    Operator op = Operator::Contains;
    QString value;                                   // Text; cents for amounts; "income"/"expense"; an account id
    QString value2;                                  // Upper end for Between (cents)

    // Text is compared without case and with spaces simplified; amounts are positive cents
    bool matches(const Transaction& transaction) const;

    static QString fieldLabel(Field field);          // For people: "Payee or description"
    static QString operatorLabel(Operator op);       // "starts with"
    static QString fieldKey(Field field);            // Database names: "payee", "payee_or_description", ...
    static QString operatorKey(Operator op);
    static std::optional<Field> fieldFromKey(const QString& key);
    static std::optional<Operator> operatorFromKey(const QString& key);
};

// "When all (or any) of these conditions match, set this category"
struct Rule {
    int id = -1;
    int position = 0;                                // Lower is tried first
    bool matchAll = true;                            // false: any condition is enough
    // What the rule does, at least one of them
    int categoryId = -1;                             // Sets this category; -1 = doesn't
    QString newPayee;                                // Renames the payee to this; empty = doesn't
    QList<RuleCondition> conditions;

    // A rule without conditions never matches. Payee conditions also look at the original payee,
    // so a renamed transaction still matches rules written for the bank's name.
    bool matches(const Transaction& transaction) const;
    bool setsCategory() const { return categoryId > 0; }
    bool renames() const { return !newPayee.isEmpty(); }
};

// Many transactions with their texts tidied once (spacing and case), so rules can be counted over
// them quickly, e.g. on each key press while a rule is edited. Counts the way Rule::matches matches.
class RuleMatchIndex {
public:
    RuleMatchIndex() = default;
    explicit RuleMatchIndex(const QList<Transaction>& transactions);

    int count(const Rule& rule) const;               // How many of the transactions the rule matches

private:
    struct Entry {
        Transaction transaction;
        QString payee;                               // Simplified and case-folded
        QString originalPayee;
        QString description;
    };
    QList<Entry> entries;
};

// What applying the rules would change on a transaction
struct RuleChange {
    Transaction transaction;                         // As it is now
    int categoryId = -1;                             // New category; -1 = unchanged
    QString payee;                                   // New payee; empty = unchanged
    int renameRuleId = -1;                           // The rule that gives the new payee
};

namespace Rules {

// For each action, the first rule (in list order) that matches and has that action, or nullptr
const Rule* categoryRule(const QList<Rule>& rules, const Transaction& transaction);
const Rule* renameRule(const QList<Rule>& rules, const Transaction& transaction);

// The category the rules give the transaction, if any
std::optional<int> categoryFor(const QList<Rule>& rules, const Transaction& transaction);

// "Payee or description contains "REWE" and amount is between 40,00 € and 120,00 €", in the
// current currency format; accountNames names the accounts of account conditions
QString describe(const Rule& rule, const QHash<int, QString>& accountNames);

// What applying the rules to these transactions would change. Transactions in a protected
// category (the balance adjustments) are left alone; with onlyUncategorised, transactions that
// have a category keep it (their payee can still be renamed).
QList<RuleChange> changesFor(const QList<Rule>& rules, const QList<Transaction>& transactions,
                             bool onlyUncategorised, const QList<int>& protectedCategories);

// Saves the changes, all or none
bool apply(const QList<RuleChange>& changes);

// A first draft of a rule for transactions like this one: its payee (or description, or amount)
// and its category
Rule draftFrom(const Transaction& transaction);

} // namespace Rules

// A user's rules, in the order they're tried
class RuleDAO {
public:
    explicit RuleDAO(int userId);

    QList<Rule> rules();                             // Ordered by position
    int addRule(const Rule& rule);                   // Added at the end; returns the id, -1 on failure
    bool updateRule(const Rule& rule);               // Conditions, actions and match mode; not the position
    bool deleteRule(int ruleId);
    bool setOrder(const QList<int>& ruleIds);        // New positions, first is tried first

private:
    bool writeConditions(int ruleId, const QList<RuleCondition>& conditions);
    bool owns(int ruleId);
    bool ownsCategory(int categoryId);
    bool valid(const Rule& rule);
    static void bindActions(QSqlQuery& query, const Rule& rule);

    int userId;
};

#endif // RULE_H
