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
    int categoryId = -1;
    QList<RuleCondition> conditions;

    bool matches(const Transaction& transaction) const;   // A rule without conditions never matches
};

// A transaction a rule would give another category
struct RuleChange {
    Transaction transaction;                         // As it is now
    int categoryId = -1;                             // What the rules give it
};

namespace Rules {

// The first rule (in list order) that matches, or nullptr
const Rule* firstMatch(const QList<Rule>& rules, const Transaction& transaction);

// The category of the first rule (in list order) that matches, if any
std::optional<int> categoryFor(const QList<Rule>& rules, const Transaction& transaction);

// "Payee or description contains "REWE" and amount is between 40,00 € and 120,00 €", in the
// current currency format; accountNames names the accounts of account conditions
QString describe(const Rule& rule, const QHash<int, QString>& accountNames);

// What applying the rules to these transactions would change. Transactions in a protected
// category (the balance adjustments) keep theirs; with onlyUncategorised, so does every
// transaction that has a category.
QList<RuleChange> changesFor(const QList<Rule>& rules, const QList<Transaction>& transactions,
                             bool onlyUncategorised, const QList<int>& protectedCategories);

// Saves the changes, all or none
bool apply(const QList<RuleChange>& changes);

} // namespace Rules

// A user's rules, in the order they're tried
class RuleDAO {
public:
    explicit RuleDAO(int userId);

    QList<Rule> rules();                             // Ordered by position
    int addRule(const Rule& rule);                   // Added at the end; returns the id, -1 on failure
    bool updateRule(const Rule& rule);               // Conditions, category and match mode; not the position
    bool deleteRule(int ruleId);
    bool setOrder(const QList<int>& ruleIds);        // New positions, first is tried first

private:
    bool writeConditions(int ruleId, const QList<RuleCondition>& conditions);
    bool owns(int ruleId);
    bool ownsCategory(int categoryId);

    int userId;
};

#endif // RULE_H
