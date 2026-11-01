#include "rule.h"
#include "db/databasemanager.h"
#include "settings.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

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

// Already as simplified() would leave it: single spaces between words, none at the ends
bool isSimplified(QStringView text) {
    bool previousSpace = true;                       // A leading space isn't simplified
    for (const QChar c : text) {
        if (c.isSpace()) {
            if (previousSpace || c != u' ') return false;
            previousSpace = true;
        } else {
            previousSpace = false;
        }
    }
    return !previousSpace || text.isEmpty();
}

bool textMatches(const QString& text, RuleCondition::Operator op, const QString& value) {
    // Rules run over every transaction, so most texts are compared in place, ignoring case,
    // and only copied when their spacing needs tidying
    const bool simpleNeedle = isSimplified(value);
    const QString needleCopy = simpleNeedle ? QString() : value.simplified();
    const QStringView needle = simpleNeedle ? QStringView(value) : QStringView(needleCopy);
    if (needle.isEmpty()) {
        return false;                                // An empty search text would match everything
    }
    const bool simpleHaystack = isSimplified(text);
    const QString haystackCopy = simpleHaystack ? QString() : text.simplified();
    const QStringView haystack = simpleHaystack ? QStringView(text) : QStringView(haystackCopy);
    switch (op) {
    case RuleCondition::Operator::Contains: return haystack.contains(needle, Qt::CaseInsensitive);
    case RuleCondition::Operator::Is: return haystack.compare(needle, Qt::CaseInsensitive) == 0;
    case RuleCondition::Operator::StartsWith: return haystack.startsWith(needle, Qt::CaseInsensitive);
    case RuleCondition::Operator::Between: return false;
    }
    return false;
}

// For RuleMatchIndex: both texts already simplified and case-folded
bool tidiedTextMatches(const QString& haystack, RuleCondition::Operator op, const QString& needle) {
    if (needle.isEmpty()) {
        return false;
    }
    switch (op) {
    case RuleCondition::Operator::Contains: return haystack.contains(needle);
    case RuleCondition::Operator::Is: return haystack == needle;
    case RuleCondition::Operator::StartsWith: return haystack.startsWith(needle);
    case RuleCondition::Operator::Between: return false;
    }
    return false;
}

QString tidied(const QString& text) {
    return text.simplified().toCaseFolded();
}

// for e.g. " rewe markt 4711 ", so whole words are found with contains(" markt ")
QString padded(const QString& text) {
    return ' ' + tidied(text) + ' ';
}

// The runs of whole words from sources that inAll says every transaction has, as the sources write
// them; only the longest ones, not the parts of them. Runs without a letter say nothing ("4711").
QStringList sharedRuns(const QStringList& sources, const std::function<bool(const QString& needle)>& inAll) {
    QStringList runs;
    for (const QString& source : sources) {
        const QStringList words = source.simplified().split(' ', Qt::SkipEmptyParts);
        for (qsizetype start = 0; start < words.size(); ++start) {
            for (qsizetype end = words.size(); end > start; --end) {
                const QString run = words.mid(start, end - start).join(' ');
                if (inAll(padded(run))) {
                    if (std::any_of(run.begin(), run.end(), [](QChar c) { return c.isLetter(); })) runs << run;
                    break;                           // Shorter ones from this word are parts of it
                }
            }
        }
    }
    QStringList longest;
    for (const QString& run : std::as_const(runs)) {
        const QString needle = padded(run);
        const bool part = std::any_of(runs.begin(), runs.end(), [&](const QString& other) {
            return other.size() > run.size() && padded(other).contains(needle);
        });
        if (!part && !longest.contains(run, Qt::CaseInsensitive)) longest << run;
    }
    return longest;
}

bool isTextField(RuleCondition::Field field) {
    return field == RuleCondition::Field::Payee || field == RuleCondition::Field::Description
           || field == RuleCondition::Field::PayeeOrDescription;
}

// Whether one of the field's texts passes the test; the payee is also tried by its original name
template <typename Test>
bool anyFieldText(RuleCondition::Field field, const QString& payee, const QString& originalPayee,
                  const QString& description, Test test) {
    switch (field) {
    case RuleCondition::Field::Payee: return test(payee) || test(originalPayee);
    case RuleCondition::Field::Description: return test(description);
    case RuleCondition::Field::PayeeOrDescription: return test(payee) || test(originalPayee) || test(description);
    default: return false;
    }
}

// All (or any) of the rule's conditions pass the test, which gets each condition and its index
template <typename Test>
bool conditionsMatch(const Rule& rule, Test test) {
    if (rule.conditions.isEmpty()) {
        return false;
    }
    for (qsizetype i = 0; i < rule.conditions.size(); ++i) {
        const bool hit = test(rule.conditions[i], i);
        if (rule.matchAll && !hit) return false;
        if (!rule.matchAll && hit) return true;
    }
    return rule.matchAll;
}

} // namespace

// --- RuleCondition ---

bool RuleCondition::matches(const Transaction& transaction) const {
    switch (field) {
    case Field::Payee:
    case Field::Description:
    case Field::PayeeOrDescription:
        return anyFieldText(field, transaction.getPayee(), transaction.getOriginalPayee(), transaction.getDescription(),
                            [this](const QString& text) { return textMatches(text, op, value); });
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
    return conditionsMatch(*this, [&transaction](const RuleCondition& condition, qsizetype) {
        return condition.matches(transaction);
    });
}

// --- RuleMatchIndex ---

RuleMatchIndex::RuleMatchIndex(const QList<Transaction>& transactions) {
    entries.reserve(transactions.size());
    for (const Transaction& transaction : transactions) {
        entries.append({transaction, tidied(transaction.getPayee()), tidied(transaction.getOriginalPayee()),
                        tidied(transaction.getDescription())});
    }
}

int RuleMatchIndex::count(const Rule& rule) const {
    QStringList needles;                             // Each condition's text, tidied once
    for (const RuleCondition& condition : rule.conditions) {
        needles.append(isTextField(condition.field) ? tidied(condition.value) : QString());
    }
    int count = 0;
    for (const Entry& entry : entries) {
        const bool hit = conditionsMatch(rule, [&](const RuleCondition& condition, qsizetype index) {
            if (!isTextField(condition.field)) {
                return condition.matches(entry.transaction);
            }
            return anyFieldText(condition.field, entry.payee, entry.originalPayee, entry.description,
                                [&](const QString& text) { return tidiedTextMatches(text, condition.op, needles[index]); });
        });
        if (hit) ++count;
    }
    return count;
}

const Rule* Rules::categoryRule(const QList<Rule>& rules, const Transaction& transaction) {
    for (const Rule& rule : rules) {
        if (rule.setsCategory() && rule.matches(transaction)) {
            return &rule;
        }
    }
    return nullptr;
}

const Rule* Rules::renameRule(const QList<Rule>& rules, const Transaction& transaction) {
    for (const Rule& rule : rules) {
        if (rule.renames() && rule.matches(transaction)) {
            return &rule;
        }
    }
    return nullptr;
}

std::optional<int> Rules::categoryFor(const QList<Rule>& rules, const Transaction& transaction) {
    const Rule* rule = categoryRule(rules, transaction);
    return rule ? std::optional<int>(rule->categoryId) : std::nullopt;
}

QString Rules::title(const Rule& rule, const QHash<int, QString>& accountNames) {
    const QString name = rule.name.simplified();
    return name.isEmpty() ? describe(rule, accountNames) : name;
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
        if (protectedCategories.contains(current)) {
            continue;
        }
        RuleChange change{transaction, -1, QString(), -1};
        if (!(onlyUncategorised && current > 0)) {
            const std::optional<int> category = categoryFor(rules, transaction);
            if (category && *category != current) {
                change.categoryId = *category;
            }
        }
        if (const Rule* rename = renameRule(rules, transaction); rename && rename->newPayee != transaction.getPayee()) {
            change.payee = rename->newPayee;
            change.renameRuleId = rename->id;
        }
        if (change.categoryId > 0 || !change.payee.isEmpty()) {
            changes.append(change);
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
        if (change.categoryId > 0) {
            changed.setCategoryId(change.categoryId);
        }
        if (!change.payee.isEmpty()) {
            changed.renamePayee(change.payee, change.renameRuleId);  // Keeps the bank's name as the original
        }
        if (!dao.updateTransaction(changed)) {
            db.rollback();
            return false;
        }
    }
    return DatabaseManager::commitOrRollback(db);
}

// "REWE SAGT DANKE 0815" → "REWE SAGT DANKE": a branch or reference number at the end only matches
// that one store or booking. Only groups of 4 digits or more, so "Studio 54" stays; a payee that's
// only numbers stays too.
static QString withoutTrailingNumbers(const QString& text) {
    QStringList words = text.simplified().split(' ', Qt::SkipEmptyParts);
    while (words.size() > 1 && words.last().size() >= 4
           && std::all_of(words.last().begin(), words.last().end(), [](QChar c) { return c.isDigit(); })) {
        words.removeLast();
    }
    return words.join(' ');
}

Rule Rules::draftFrom(const Transaction& transaction) {
    Rule rule;
    rule.categoryId = transaction.getCategoryId() > 0 ? transaction.getCategoryId() : -1;
    const QString payee = withoutTrailingNumbers(transaction.getPayee());
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

Rule Rules::draftFrom(const QList<Transaction>& transactions, const MatchCounter& count) {
    if (transactions.size() <= 1) {
        return transactions.isEmpty() ? Rule() : draftFrom(transactions.first());
    }
    Rule rule;
    QHash<int, int> categoryCounts;
    int mostCommon = 0;
    for (const Transaction& transaction : transactions) {
        const int categoryId = transaction.getCategoryId();
        if (categoryId <= 0) continue;
        const int seen = ++categoryCounts[categoryId];
        if (seen > mostCommon) {                     // Ties: the one that got there first
            rule.categoryId = categoryId;
            mostCommon = seen;
        }
    }

    // Text they all share, in the payee, the description, or either of them. Descriptions often share
    // the bank's wording too ("Kartenzahlung"), so the text that matches the fewest of the user's
    // transactions is the one meant; on a tie the payee, then the description.
    using Field = RuleCondition::Field;
    QStringList payees, descriptions;
    for (const Transaction& transaction : transactions) {
        payees << padded(transaction.getPayee());
        descriptions << padded(transaction.getDescription());
    }
    const auto inAllPayees = [&](const QString& needle) {
        return std::all_of(payees.begin(), payees.end(), [&](const QString& text) { return text.contains(needle); });
    };
    const auto inAllDescriptions = [&](const QString& needle) {
        return std::all_of(descriptions.begin(), descriptions.end(), [&](const QString& text) { return text.contains(needle); });
    };
    const auto inAllEither = [&](const QString& needle) {
        for (qsizetype i = 0; i < payees.size(); ++i) {
            if (!payees[i].contains(needle) && !descriptions[i].contains(needle)) return false;
        }
        return true;
    };
    const Transaction& first = transactions.first();
    const QList<QPair<Field, QStringList>> candidates = {
        {Field::Payee, sharedRuns({first.getPayee()}, inAllPayees)},
        {Field::Description, sharedRuns({first.getDescription()}, inAllDescriptions)},
        {Field::PayeeOrDescription, sharedRuns({first.getPayee(), first.getDescription()}, inAllEither)},
    };
    std::optional<RuleCondition> best;
    int bestMatches = 0;
    for (const auto& [field, runs] : candidates) {
        for (const QString& run : runs) {
            Rule candidate;
            candidate.conditions = {RuleCondition(field, RuleCondition::Operator::Contains, run)};
            const int matches = count ? count(candidate) : 0;
            if (!best || matches < bestMatches || (matches == bestMatches && field == best->field && run.size() > best->value.size())) {
                best = candidate.conditions.first();
                bestMatches = matches;
            }
        }
    }
    if (best) {
        rule.conditions = {*best};
        return rule;
    }
    rule.matchAll = false;
    for (const Transaction& transaction : transactions) {
        if (!rule.matches(transaction)) rule.conditions += draftFrom(transaction).conditions;
    }
    return rule;
}

bool Rules::canAddTo(const Rule& rule) {
    return !rule.matchAll || rule.conditions.size() <= 1;
}

Rule Rules::addedTo(Rule rule, const QList<Transaction>& transactions, const MatchCounter& count) {
    QList<Transaction> missing;
    for (const Transaction& transaction : transactions) {
        if (!rule.matches(transaction)) missing << transaction;
    }
    if (!missing.isEmpty()) {
        rule.matchAll = false;
        rule.conditions += draftFrom(missing, count).conditions;
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
    query.prepare("SELECT id, position, match_all, category_id, new_payee, name FROM rules WHERE user_id = :user_id "
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
        rule.categoryId = query.value(3).isNull() ? -1 : query.value(3).toInt();
        rule.newPayee = query.value(4).toString();
        rule.name = query.value(5).toString();
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

// A rule does something (a category of this user, a new payee, or both) when its conditions match
bool RuleDAO::valid(const Rule& rule) {
    if (!rule.setsCategory() && !rule.renames()) {
        return false;
    }
    return !rule.setsCategory() || ownsCategory(rule.categoryId);
}

void RuleDAO::bindActions(QSqlQuery& query, const Rule& rule) {
    query.bindValue(":category_id", rule.setsCategory() ? QVariant(rule.categoryId) : QVariant(QMetaType(QMetaType::Int)));
    query.bindValue(":new_payee", rule.renames() ? QVariant(rule.newPayee) : QVariant(QMetaType(QMetaType::QString)));
}

void RuleDAO::bindName(QSqlQuery& query, const Rule& rule) {
    const QString name = rule.name.simplified();
    query.bindValue(":name", name.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(name));
}

int RuleDAO::addRule(const Rule& rule) {
    if (!valid(rule)) {
        return -1;
    }
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return -1;
    }
    QSqlQuery query(db);
    query.prepare("INSERT INTO rules (user_id, position, name, match_all, category_id, new_payee) "
                  "VALUES (:user_id, (SELECT COALESCE(MAX(position), -1) + 1 FROM rules WHERE user_id = :user_id2), "
                  ":name, :match_all, :category_id, :new_payee)");
    query.bindValue(":user_id", userId);
    query.bindValue(":user_id2", userId);
    query.bindValue(":match_all", rule.matchAll ? 1 : 0);
    bindName(query, rule);
    bindActions(query, rule);
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
    if (!owns(rule.id) || !valid(rule)) {
        return false;
    }
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.transaction()) {
        return false;
    }
    QSqlQuery query(db);
    query.prepare("UPDATE rules SET name = :name, match_all = :match_all, category_id = :category_id, "
                  "new_payee = :new_payee WHERE id = :id");
    query.bindValue(":match_all", rule.matchAll ? 1 : 0);
    bindName(query, rule);
    bindActions(query, rule);
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
    return DatabaseManager::commitOrRollback(db);
}
