#include <QtTest>

#include "account.h"
#include "category.h"
#include "rule.h"
#include "tests/test_db_fixture.h"

using Field = RuleCondition::Field;
using Operator = RuleCondition::Operator;

class TestRule : public QObject {
    Q_OBJECT
    TestDbFixture db;

    static Transaction transaction(const QString& payee, const QString& description, Cents amount,
                                   const QString& type = "expense", int accountId = 1) {
        return Transaction(-1, accountId, payee, amount, QDate(2026, 10, 1), description, 0, type);
    }
    static int categoryId(int userId, const QString& name) {
        for (const Category& category : CategoryDAO(userId).getAllCategories()) {
            if (category.getName() == name) return category.getCategoryId();
        }
        return -1;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void textConditionsIgnoreCaseAndSpaces() {
        const Transaction rewe = transaction("REWE  Markt GmbH", "Kartenzahlung", 3240);
        QVERIFY((RuleCondition{Field::Payee, Operator::Contains, "rewe markt"}.matches(rewe)));
        QVERIFY((RuleCondition{Field::Payee, Operator::StartsWith, "Rewe"}.matches(rewe)));
        QVERIFY((RuleCondition{Field::Payee, Operator::Is, "rewe markt gmbh"}.matches(rewe)));
        QVERIFY(!(RuleCondition{Field::Payee, Operator::Is, "rewe"}.matches(rewe)));
        QVERIFY((RuleCondition{Field::Description, Operator::Contains, "karte"}.matches(rewe)));
        QVERIFY(!(RuleCondition{Field::Description, Operator::Contains, "rewe"}.matches(rewe)));
        QVERIFY(!(RuleCondition{Field::Payee, Operator::Contains, "  "}.matches(rewe)));   // Empty matches nothing

        // Umlauts ignore case too; tabs, line breaks and spaces at the ends count as single spaces
        const Transaction doctor = transaction("Ärztehaus Mitte", "Rechnung\n04/2026 \t Praxis", 9000);
        QVERIFY((RuleCondition{Field::Payee, Operator::StartsWith, "ärzte"}.matches(doctor)));
        QVERIFY((RuleCondition{Field::Payee, Operator::Is, " ärztehaus\tmitte "}.matches(doctor)));
        QVERIFY((RuleCondition{Field::Description, Operator::Contains, "rechnung 04/2026 praxis"}.matches(doctor)));
        QVERIFY(!(RuleCondition{Field::Description, Operator::Contains, "rechnung  04"}.matches(transaction("x", "Rechnung04", 1))));
    }

    void theMatchIndexCountsLikeTheRulesMatch() {
        // The quick counter of the Rules page has to agree with matching one transaction at a time
        Transaction renamed = transaction("REWE  City 0815", "", 990);
        renamed.renamePayee("Rewe", -1);
        const QList<Transaction> transactions = {
            transaction("REWE  Markt GmbH", "Kartenzahlung", 3240), transaction("Aldi Süd", "Einkauf\tbei REWE", 1500),
            transaction("Ärztehaus Mitte", "Rechnung 04/2026", 9000, "expense", 2), transaction("Arbeitgeber", "Gehalt", 300000, "income"),
            renamed, transaction("", "", 100)};
        QList<Rule> rules;
        for (const QList<RuleCondition>& conditions : QList<QList<RuleCondition>>{
                 {{Field::Payee, Operator::Contains, "rewe"}},
                 {{Field::Payee, Operator::Is, "rewe city 0815"}},
                 {{Field::PayeeOrDescription, Operator::Contains, "bei  rewe"}},
                 {{Field::Description, Operator::StartsWith, "RECHNUNG"}},
                 {{Field::Payee, Operator::StartsWith, "ärzte"}, {Field::Account, Operator::Is, "2"}},
                 {{Field::Type, Operator::Is, "income"}, {Field::Amount, Operator::Between, "100000", "500000"}},
                 {{Field::Payee, Operator::Contains, "  "}},
             }) {
            for (const bool matchAll : {true, false}) {
                Rule rule;
                rule.matchAll = matchAll;
                rule.conditions = conditions;
                rules.append(rule);
            }
        }
        rules.append(Rule());                            // No conditions: matches nothing
        const RuleMatchIndex index(transactions);
        for (const Rule& rule : std::as_const(rules)) {
            int expected = 0;
            for (const Transaction& candidate : transactions) expected += rule.matches(candidate) ? 1 : 0;
            QCOMPARE(index.count(rule), expected);
        }
        QCOMPARE(index.count(rules[2]), 1);              // The renamed one, by its original name
    }

    void payeeOrDescriptionLooksAtBoth() {
        const RuleCondition rewe{Field::PayeeOrDescription, Operator::Contains, "REWE"};
        QVERIFY(rewe.matches(transaction("REWE Markt", "", 100)));
        QVERIFY(rewe.matches(transaction("Visa", "Einkauf REWE 4711", 100)));
        QVERIFY(!rewe.matches(transaction("Aldi", "Einkauf", 100)));
    }

    void amountTypeAndAccount() {
        const Transaction bill = transaction("Stadtwerke", "", 8000, "expense", 7);
        QVERIFY((RuleCondition{Field::Amount, Operator::Is, "8000"}.matches(bill)));
        QVERIFY((RuleCondition{Field::Amount, Operator::Between, "4000", "12000"}.matches(bill)));
        QVERIFY((RuleCondition{Field::Amount, Operator::Between, "12000", "4000"}.matches(bill)));   // Either order
        QVERIFY((RuleCondition{Field::Amount, Operator::Between, "8000", "8000"}.matches(bill)));    // Ends included
        QVERIFY(!(RuleCondition{Field::Amount, Operator::Between, "8001", "9000"}.matches(bill)));
        QVERIFY((RuleCondition{Field::Type, Operator::Is, "expense"}.matches(bill)));
        QVERIFY(!(RuleCondition{Field::Type, Operator::Is, "income"}.matches(bill)));
        QVERIFY((RuleCondition{Field::Account, Operator::Is, "7"}.matches(bill)));
        QVERIFY(!(RuleCondition{Field::Account, Operator::Is, "8"}.matches(bill)));
    }

    void allOrAnyCondition() {
        Rule rule;
        rule.conditions = {{Field::Payee, Operator::Contains, "Stadtwerke"},
                           {Field::Amount, Operator::Between, "4000", "12000"}};
        QVERIFY(rule.matches(transaction("Stadtwerke", "", 8000)));
        QVERIFY(!rule.matches(transaction("Stadtwerke", "", 30000)));
        rule.matchAll = false;
        QVERIFY(rule.matches(transaction("Stadtwerke", "", 30000)));
        QVERIFY(rule.matches(transaction("Aldi", "", 8000)));
        QVERIFY(!rule.matches(transaction("Aldi", "", 30000)));
        rule.conditions.clear();
        QVERIFY(!rule.matches(transaction("Anything", "", 1)));      // No conditions: never
    }

    void theFirstMatchingRuleWins() {
        Rule groceries;
        groceries.categoryId = 1;
        groceries.conditions = {{Field::PayeeOrDescription, Operator::Contains, "REWE"}};
        Rule anyExpense;
        anyExpense.categoryId = 2;
        anyExpense.conditions = {{Field::Type, Operator::Is, "expense"}};
        QCOMPARE(Rules::categoryFor({groceries, anyExpense}, transaction("REWE", "", 100)), std::optional<int>(1));
        QCOMPARE(Rules::categoryFor({anyExpense, groceries}, transaction("REWE", "", 100)), std::optional<int>(2));
        QCOMPARE(Rules::categoryFor({groceries}, transaction("Aldi", "", 100)), std::nullopt);
    }

    void changesSkipProtectedAndUnchangedTransactions() {
        Rule groceries;
        groceries.categoryId = 10;
        groceries.conditions = {{Field::PayeeOrDescription, Operator::Contains, "REWE"}};
        Transaction uncategorised = transaction("REWE", "", 100);
        Transaction other = transaction("REWE", "", 200);
        other.setCategoryId(20);
        Transaction already = transaction("REWE", "", 300);
        already.setCategoryId(10);
        Transaction start = transaction("REWE", "Start", 400);
        start.setCategoryId(99);                         // A balance adjustment
        Transaction unmatched = transaction("Aldi", "", 500);
        const QList<Transaction> all = {uncategorised, other, already, start, unmatched};

        QList<RuleChange> changes = Rules::changesFor({groceries}, all, true, {99});
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.first().transaction.getAmountCents(), Cents(100));
        changes = Rules::changesFor({groceries}, all, false, {99});
        QCOMPARE(changes.size(), 2);                     // Also the one in category 20, not 10 or 99
        QCOMPARE(changes.last().transaction.getAmountCents(), Cents(200));
        QCOMPARE(changes.last().categoryId, 10);
    }

    void applyingSavesTheNewCategories() {
        const int account = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        TransactionDAO dao;
        const int id = dao.addTransaction(Transaction(-1, account, "REWE", 3240, QDate(2026, 10, 1), "", 0, "expense"));
        RuleChange change{dao.getTransactionById(id), categoryId(1, "Groceries"), QString(), -1};
        QVERIFY(Rules::apply({change}));
        QCOMPARE(dao.getTransactionById(id).getCategoryId(), categoryId(1, "Groceries"));
        QCOMPARE(dao.getTransactionById(id).getAmountCents(), Cents(3240));   // Nothing else changed
    }

    void draftsStartFromThePayeeThenDescriptionThenAmount() {
        Transaction rewe = transaction("  REWE   Markt ", "Wocheneinkauf", 3240);
        rewe.setCategoryId(7);
        Rule draft = Rules::draftFrom(rewe);
        QCOMPARE(draft.categoryId, 7);
        QCOMPARE(draft.conditions.size(), 1);
        QCOMPARE(draft.conditions[0].field, Field::Payee);
        QCOMPARE(draft.conditions[0].value, QString("REWE Markt"));
        QVERIFY(draft.matches(rewe));

        draft = Rules::draftFrom(transaction("", "Gehalt Oktober", 245000, "income"));
        QCOMPARE(draft.conditions[0].field, Field::Description);
        QCOMPARE(draft.categoryId, -1);                  // No category yet: chosen in the editor

        draft = Rules::draftFrom(transaction("", "", 999));
        QCOMPARE(draft.conditions[0].field, Field::Amount);
        QCOMPARE(draft.conditions[0].value, QString("999"));
    }

    void renamingKeepsTheBanksNameForMatchingAndDuplicates() {
        Transaction rewe = transaction("REWE MARKT GMBH 4711", "", 3240);
        const QString bankHash = rewe.calculateHash();
        rewe.renamePayee("Rewe", 1);
        rewe.renamePayee("REWE", 2);                     // A second rename keeps the first original
        QCOMPARE(rewe.getPayee(), QString("REWE"));
        QCOMPARE(rewe.getRenamedByRuleId(), 2);
        QCOMPARE(rewe.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));
        QCOMPARE(rewe.calculateHash(), bankHash);
        QVERIFY((RuleCondition(Field::Payee, Operator::Contains, "markt gmbh").matches(rewe)));
        QVERIFY((RuleCondition(Field::PayeeOrDescription, Operator::StartsWith, "rewe markt").matches(rewe)));
    }

    void eachActionTakesTheFirstRuleThatHasIt() {
        Rule rename;
        rename.newPayee = "Rewe";
        rename.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        Rule category;
        category.categoryId = 10;
        category.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        const QList<Rule> rules = {rename, category};
        const Transaction rewe = transaction("REWE MARKT", "", 100);
        QCOMPARE(Rules::categoryFor(rules, rewe), std::optional<int>(10));   // The rename rule doesn't block it
        QCOMPARE(Rules::renameRule(rules, rewe)->newPayee, QString("Rewe"));

        const QList<RuleChange> changes = Rules::changesFor(rules, {rewe}, true, {});
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.first().categoryId, 10);
        QCOMPARE(changes.first().payee, QString("Rewe"));

        Transaction categorised = rewe;
        categorised.setCategoryId(20);                   // Keeps its category, still renamed
        const RuleChange onlyRename = Rules::changesFor(rules, {categorised}, true, {}).first();
        QCOMPARE(onlyRename.categoryId, -1);
        QCOMPARE(onlyRename.payee, QString("Rewe"));
        Transaction renamed = rewe;
        renamed.renamePayee("Rewe", 1);
        renamed.setCategoryId(10);
        QVERIFY(Rules::changesFor(rules, {renamed}, false, {}).isEmpty());   // Nothing left to do
    }

    void applyingRenamesAndKeepsTheOriginal() {
        const int account = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        TransactionDAO dao;
        const Transaction bankRow(-1, account, "REWE MARKT GMBH 4711", 3240, QDate(2026, 10, 1), "", 0, "expense");
        const int id = dao.addTransaction(bankRow);
        Rule rule;
        rule.newPayee = "Rewe";
        rule.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        RuleDAO rules(1);
        rule.id = rules.addRule(rule);
        QVERIFY(rule.id > 0);

        const QList<RuleChange> changes = Rules::changesFor({rule}, {dao.getTransactionById(id)}, false, {});
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.first().renameRuleId, rule.id);
        QVERIFY(Rules::apply(changes));
        const Transaction saved = dao.getTransactionById(id);
        QCOMPARE(saved.getPayee(), QString("Rewe"));
        QCOMPARE(saved.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));
        QCOMPARE(saved.getRenamedByRuleId(), rule.id);   // Which rule did it
        QCOMPARE(dao.getExistingHashes({bankRow.calculateHash()}).size(), 1);   // Still a duplicate of the bank's row

        // Deleting the rule keeps the name, but nothing points to the rule anymore
        QVERIFY(rules.deleteRule(rule.id));
        QCOMPARE(dao.getTransactionById(id).getPayee(), QString("Rewe"));
        QCOMPARE(dao.getTransactionById(id).getRenamedByRuleId(), -1);
    }

    void rulesNeedAnActionAndKeepTheirRenameWithoutACategory() {
        RuleDAO dao(1);
        Rule nothing;
        nothing.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        QVERIFY(dao.addRule(nothing) <= 0);              // Neither category nor rename

        CategoryDAO categories(1);
        const int travel = categories.addCategory(Category(-1, "Travel", "expense"));
        Rule onlyCategory = nothing;
        onlyCategory.categoryId = travel;
        Rule both = onlyCategory;
        both.newPayee = "Deutsche Bahn";
        Rule onlyRename = nothing;
        onlyRename.newPayee = "Rewe";
        QVERIFY(dao.addRule(onlyCategory) > 0);
        QVERIFY(dao.addRule(both) > 0);
        QVERIFY(dao.addRule(onlyRename) > 0);
        QCOMPARE(dao.rules()[2].categoryId, -1);
        QCOMPARE(dao.rules()[2].newPayee, QString("Rewe"));

        QVERIFY(categories.deleteCategory(travel));      // Unused category
        const QList<Rule> left = dao.rules();
        QCOMPARE(left.size(), 2);                        // The category-only rule is gone
        QCOMPARE(left[0].newPayee, QString("Deutsche Bahn"));
        QCOMPARE(left[0].categoryId, -1);                // The other one keeps renaming
    }

    void rulesAreStoredWithTheirConditionsAndOrder() {
        RuleDAO dao(1);
        Rule groceries;
        groceries.categoryId = categoryId(1, "Groceries");
        groceries.conditions = {{Field::PayeeOrDescription, Operator::Contains, "REWE"}};
        Rule utilities;
        utilities.categoryId = categoryId(1, "Utilities");
        utilities.matchAll = false;
        utilities.conditions = {{Field::Payee, Operator::Contains, "Stadtwerke"},
                                {Field::Amount, Operator::Between, "4000", "12000"}};
        const int first = dao.addRule(groceries);
        const int second = dao.addRule(utilities);
        QVERIFY(first > 0 && second > 0);

        QList<Rule> rules = dao.rules();
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules[0].id, first);                        // Added at the end
        QCOMPARE(rules[1].matchAll, false);
        QCOMPARE(rules[1].conditions.size(), 2);
        QCOMPARE(rules[1].conditions[1].field, Field::Amount);
        QCOMPARE(rules[1].conditions[1].value2, QString("12000"));

        QVERIFY(dao.setOrder({second, first}));
        QCOMPARE(dao.rules().first().id, second);

        rules[0].conditions = {{Field::Payee, Operator::Is, "Rewe"}};
        rules[0].categoryId = categoryId(1, "Entertainment");
        QVERIFY(dao.updateRule(rules[0]));
        const Rule updated = dao.rules()[1];
        QCOMPARE(updated.conditions.size(), 1);
        QCOMPARE(updated.conditions[0].op, Operator::Is);
        QCOMPARE(updated.categoryId, categoryId(1, "Entertainment"));

        QVERIFY(dao.deleteRule(first));
        QCOMPARE(dao.rules().size(), 1);
    }

    void rulesCanHaveANameOrBeShownByTheirConditions() {
        RuleDAO dao(1);
        Rule rule;
        rule.categoryId = categoryId(1, "Groceries");
        rule.name = "  Super   markets ";
        rule.conditions = {{Field::Payee, Operator::Contains, "Aldi"}};
        rule.id = dao.addRule(rule);
        QVERIFY(rule.id > 0);
        QCOMPARE(dao.rules().first().name, QString("Super markets"));       // Spaces tidied
        QCOMPARE(Rules::title(dao.rules().first(), {}), QString("Super markets"));

        rule.name = "Supermarkets";
        QVERIFY(dao.updateRule(rule));
        QCOMPARE(dao.rules().first().name, QString("Supermarkets"));

        rule.name = "   ";
        QVERIFY(dao.updateRule(rule));
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT name IS NULL FROM rules") && query.next());
        QVERIFY(query.value(0).toBool());                                   // No name is stored as none
        QCOMPARE(Rules::title(dao.rules().first(), {}), QString("Payee contains \"Aldi\""));
    }

    void usersDontSeeOrChangeEachOthersRules() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        Rule rule;
        rule.categoryId = categoryId(1, "Groceries");
        rule.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        const int mine = RuleDAO(1).addRule(rule);
        QVERIFY(mine > 0);
        QVERIFY(RuleDAO(2).rules().isEmpty());
        rule.id = mine;
        rule.categoryId = categoryId(2, "Groceries");
        QVERIFY(!RuleDAO(2).updateRule(rule));
        QVERIFY(!RuleDAO(2).deleteRule(mine));
        QVERIFY(!RuleDAO(2).setOrder({mine}));
        QVERIFY(RuleDAO(1).addRule(rule) <= 0);              // Sam's category isn't user 1's
    }

    void rulesFollowTheirCategory() {
        CategoryDAO categories(1);
        const int from = categories.addCategory(Category(-1, "Lebensmittel", "expense"));
        const int into = categoryId(1, "Groceries");
        Rule rule;
        rule.categoryId = from;
        rule.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        QVERIFY(RuleDAO(1).addRule(rule) > 0);

        QVERIFY(categories.mergeCategory(from, into));       // Merging moves the rule along
        QCOMPARE(RuleDAO(1).rules().first().categoryId, into);

        QVERIFY(categories.deleteCategoryKeepingTransactions(into));   // Deleting removes it
        QVERIFY(RuleDAO(1).rules().isEmpty());
    }
};

QTEST_MAIN(TestRule)
#include "test_rule.moc"
