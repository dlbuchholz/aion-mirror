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
