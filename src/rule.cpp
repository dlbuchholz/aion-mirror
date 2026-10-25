#include "rule.h"
#include "db/databasemanager.h"
#include "settings.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace {

const QList<QPair<RuleCondition::Field, QString>> FieldKeys = {
    {RuleCondition::Field::Payee, "payee"},
    {RuleCondition::Field::Description, "description"},
    {RuleCondition::Field::PayeeOrDescription, "payee_or_description"},
    {RuleCondition::Field::Amount, "amount"},
    {RuleCondition::Field::Type, "type"},
    {RuleCondition::Field::Account, "account"},
};

const QList<QPair<RuleCondition::Operator, QString>> OperatorKeys = {
    {RuleCondition::Operator::Contains, "contains"},
    {RuleCondition::Operator::Is, "is"},
    {RuleCondition::Operator::StartsWith, "starts_with"},
    {RuleCondition::Operator::Between, "between"},
};

QString normalized(const QString& text) {
    return text.simplified().toCaseFolded();
}

bool textMatches(const QString& text, RuleCondition::Operator op, const QString& value) {
    const QString haystack = normalized(text);
    const QString needle = normalized(value);
    if (needle.isEmpty()) {
        return false;                                // An empty search text would match everything
    }
    switch (op) {
    case RuleCondition::Operator::Contains: return haystack.contains(needle);
    case RuleCondition::Operator::Is: return haystack == needle;
    case RuleCondition::Operator::StartsWith: return haystack.startsWith(needle);
    case RuleCondition::Operator::Between: return false;
    }
    return false;
}

} // namespace

// --- RuleCondition ---

bool RuleCondition::matches(const Transaction& transaction) const {
    switch (field) {
    case Field::Payee:
        return textMatches(transaction.getPayee(), op, value);
    case Field::Description:
        return textMatches(transaction.getDescription(), op, value);
    case Field::PayeeOrDescription:
        return textMatches(transaction.getPayee(), op, value) || textMatches(transaction.getDescription(), op, value);
    case Field::Amount: {
        bool ok = false;
        const Cents low = value.toLongLong(&ok);
        if (!ok) return false;
        const Cents amount = transaction.getAmountCents();
        if (op == Operator::Is) return amount == low;
        if (op != Operator::Between) return false;
        const Cents high = value2.toLongLong(&ok);
        return ok && amount >= qMin(low, high) && amount <= qMax(low, high);
    }
    case Field::Type:
        return op == Operator::Is && transaction.getType() == value;
    case Field::Account:
        return op == Operator::Is && QString::number(transaction.getAccountId()) == value;
    }
    return false;
}

QString RuleCondition::fieldLabel(Field field) {
    switch (field) {
    case Field::Payee: return "Payee";
    case Field::Description: return "Description";
    case Field::PayeeOrDescription: return "Payee or description";
    case Field::Amount: return "Amount";
    case Field::Type: return "Type";
    case Field::Account: return "Account";
    }
    return {};
}

QString RuleCondition::operatorLabel(Operator op) {
    switch (op) {
    case Operator::Contains: return "contains";
    case Operator::Is: return "is";
    case Operator::StartsWith: return "starts with";
    case Operator::Between: return "is between";
    }
    return {};
}

QString RuleCondition::fieldKey(Field field) {
    for (const auto& key : FieldKeys) {
        if (key.first == field) return key.second;
    }
    return {};
}

QString RuleCondition::operatorKey(Operator op) {
    for (const auto& key : OperatorKeys) {
        if (key.first == op) return key.second;
    }
    return {};
}

std::optional<RuleCondition::Field> RuleCondition::fieldFromKey(const QString& text) {
    for (const auto& key : FieldKeys) {
        if (key.second == text) return key.first;
    }
    return std::nullopt;
}

std::optional<RuleCondition::Operator> RuleCondition::operatorFromKey(const QString& text) {
    for (const auto& key : OperatorKeys) {
        if (key.second == text) return key.first;
    }
    return std::nullopt;
}

// --- Rule ---

bool Rule::matches(const Transaction& transaction) const {
    if (conditions.isEmpty()) {
        return false;
    }
    for (const RuleCondition& condition : conditions) {
        const bool hit = condition.matches(transaction);
        if (matchAll && !hit) return false;
        if (!matchAll && hit) return true;
    }
    return matchAll;
}

const Rule* Rules::firstMatch(const QList<Rule>& rules, const Transaction& transaction) {
    for (const Rule& rule : rules) {
        if (rule.matches(transaction)) {
            return &rule;
        }
    }
    return nullptr;
}

std::optional<int> Rules::categoryFor(const QList<Rule>& rules, const Transaction& transaction) {
    const Rule* rule = firstMatch(rules, transaction);
    return rule ? std::optional<int>(rule->categoryId) : std::nullopt;
}

QString Rules::describe(const Rule& rule, const QHash<int, QString>& accountNames) {
    using Field = RuleCondition::Field;
    using Operator = RuleCondition::Operator;
    const Settings& settings = Settings::instance();
    QStringList parts;
    for (const RuleCondition& condition : rule.conditions) {
        QString text;
        switch (condition.field) {
        case Field::Amount: {
            const QString from = settings.formatCurrency(condition.value.toLongLong());
            text = condition.op == Operator::Between
                       ? QString("amount is between %1 and %2").arg(from, settings.formatCurrency(condition.value2.toLongLong()))
                       : QString("amount is %1").arg(from);
            break;
        }
        case Field::Type:
            text = condition.value == "income" ? "it's income" : "it's an expense";
            break;
        case Field::Account:
            text = QString("account is %1").arg(accountNames.value(condition.value.toInt(), "a deleted account"));
            break;
        default:
            text = QString("%1 %2 \"%3\"").arg(RuleCondition::fieldLabel(condition.field),
                                               RuleCondition::operatorLabel(condition.op), condition.value);
        }
        parts << (parts.isEmpty() ? text : text.left(1).toLower() + text.mid(1));
    }
    if (parts.isEmpty()) {
        return "No conditions";
    }
    const QString result = parts.join(rule.matchAll ? " and " : " or ");
    return result.left(1).toUpper() + result.mid(1);
}

QList<RuleChange> Rules::changesFor(const QList<Rule>& rules, const QList<Transaction>& transactions,
                                   bool onlyUncategorised, const QList<int>& protectedCategories) {
    QList<RuleChange> changes;
    for (const Transaction& transaction : transactions) {
        const int current = transaction.getCategoryId();
        if (protectedCategories.contains(current) || (onlyUncategorised && current > 0)) {
            continue;
        }
        const std::optional<int> category = categoryFor(rules, transaction);
        if (category && *category != current) {
            changes.append({transaction, *category});
        }
    }
    return changes;
}

bool Rules::apply(const QList<RuleChange>& changes) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    TransactionDAO dao;
    for (const RuleChange& change : changes) {
        Transaction changed = change.transaction;
        changed.setCategoryId(change.categoryId);
        if (!dao.updateTransaction(changed)) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

Rule Rules::draftFrom(const Transaction& transaction) {
    Rule rule;
    rule.categoryId = transaction.getCategoryId() > 0 ? transaction.getCategoryId() : -1;
    const QString payee = transaction.getPayee().simplified();
    const QString description = transaction.getDescription().simplified();
    if (!payee.isEmpty()) {
        rule.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::Contains, payee)};
    } else if (!description.isEmpty()) {
        rule.conditions = {RuleCondition(RuleCondition::Field::Description, RuleCondition::Operator::Contains, description)};
    } else {
        rule.conditions = {RuleCondition(RuleCondition::Field::Amount, RuleCondition::Operator::Is,
                                         QString::number(transaction.getAmountCents()))};
    }
    return rule;
}

// --- RuleDAO ---

RuleDAO::RuleDAO(int userId)
    : userId(userId) {}

QList<Rule> RuleDAO::rules() {
    QSqlDatabase db = DatabaseManager::instance().database();
    QList<Rule> result;
    QHash<int, qsizetype> indexById;
    QSqlQuery query(db);
    query.prepare("SELECT id, position, match_all, category_id FROM rules WHERE user_id = :user_id "
                  "ORDER BY position, id");
    query.bindValue(":user_id", userId);
    if (!query.exec()) {
        qWarning() << "RuleDAO::rules:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        Rule rule;
        rule.id = query.value(0).toInt();
        rule.position = query.value(1).toInt();
        rule.matchAll = query.value(2).toBool();
        rule.categoryId = query.value(3).toInt();
        indexById.insert(rule.id, result.size());
        result.append(rule);
    }

    query.prepare("SELECT c.rule_id, c.field, c.operator, c.value, c.value2 FROM rule_conditions c "
                  "JOIN rules r ON r.id = c.rule_id WHERE r.user_id = :user_id ORDER BY c.id");
    query.bindValue(":user_id", userId);
    if (!query.exec()) {
        qWarning() << "RuleDAO::rules:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        const auto field = RuleCondition::fieldFromKey(query.value(1).toString());
        const auto op = RuleCondition::operatorFromKey(query.value(2).toString());
        const auto index = indexById.constFind(query.value(0).toInt());
        if (!field || !op || index == indexById.constEnd()) {
            continue;
        }
        result[*index].conditions.append({*field, *op, query.value(3).toString(), query.value(4).toString()});
    }
    return result;
}

bool RuleDAO::writeConditions(int ruleId, const QList<RuleCondition>& conditions) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM rule_conditions WHERE rule_id = :rule_id");
    query.bindValue(":rule_id", ruleId);
    if (!query.exec()) {
        return false;
    }
    for (const RuleCondition& condition : conditions) {
        query.prepare("INSERT INTO rule_conditions (rule_id, field, operator, value, value2) "
                      "VALUES (:rule_id, :field, :operator, :value, :value2)");
        query.bindValue(":rule_id", ruleId);
        query.bindValue(":field", RuleCondition::fieldKey(condition.field));
        query.bindValue(":operator", RuleCondition::operatorKey(condition.op));
        query.bindValue(":value", condition.value);
        query.bindValue(":value2", condition.value2.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                              : QVariant(condition.value2));
        if (!query.exec()) {
            qWarning() << "RuleDAO: couldn't save a condition:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool RuleDAO::owns(int ruleId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT 1 FROM rules WHERE id = :id AND user_id = :user_id");
    query.bindValue(":id", ruleId);
    query.bindValue(":user_id", userId);
    return query.exec() && query.next();
}

bool RuleDAO::ownsCategory(int categoryId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT 1 FROM categories WHERE id = :id AND user_id = :user_id");
    query.bindValue(":id", categoryId);
    query.bindValue(":user_id", userId);
    return query.exec() && query.next();
}

int RuleDAO::addRule(const Rule& rule) {
    if (!ownsCategory(rule.categoryId)) {
        return -1;
    }
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO rules (user_id, position, match_all, category_id) "
                  "VALUES (:user_id, (SELECT COALESCE(MAX(position), -1) + 1 FROM rules WHERE user_id = :user_id2), "
                  ":match_all, :category_id)");
    query.bindValue(":user_id", userId);
    query.bindValue(":user_id2", userId);
    query.bindValue(":match_all", rule.matchAll ? 1 : 0);
    query.bindValue(":category_id", rule.categoryId);
    if (!query.exec()) {
        qWarning() << "RuleDAO::addRule:" << query.lastError().text();
        db.rollback();
        return -1;
    }
    const int ruleId = query.lastInsertId().toInt();
    if (!writeConditions(ruleId, rule.conditions) || !db.commit()) {
        db.rollback();
        return -1;
    }
    return ruleId;
}

bool RuleDAO::updateRule(const Rule& rule) {
    if (!owns(rule.id) || !ownsCategory(rule.categoryId)) {
        return false;
    }
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE rules SET match_all = :match_all, category_id = :category_id WHERE id = :id");
    query.bindValue(":match_all", rule.matchAll ? 1 : 0);
    query.bindValue(":category_id", rule.categoryId);
    query.bindValue(":id", rule.id);
    if (!query.exec() || !writeConditions(rule.id, rule.conditions) || !db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}

bool RuleDAO::deleteRule(int ruleId) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("DELETE FROM rules WHERE id = :id AND user_id = :user_id");
    query.bindValue(":id", ruleId);
    query.bindValue(":user_id", userId);
    return query.exec() && query.numRowsAffected() == 1;
}

bool RuleDAO::setOrder(const QList<int>& ruleIds) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    QSqlQuery query(db);
    for (int position = 0; position < ruleIds.size(); ++position) {
        query.prepare("UPDATE rules SET position = :position WHERE id = :id AND user_id = :user_id");
        query.bindValue(":position", position);
        query.bindValue(":id", ruleIds[position]);
        query.bindValue(":user_id", userId);
        if (!query.exec() || query.numRowsAffected() != 1) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}
