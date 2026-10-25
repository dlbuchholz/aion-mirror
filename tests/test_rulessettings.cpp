#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>

#include "account.h"
#include "category.h"
#include "rule.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/rulessettings.h"

using Field = RuleCondition::Field;
using Operator = RuleCondition::Operator;

class TestRulesSettings : public QObject {
    Q_OBJECT
    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;
    int giroId = -1;

    static int categoryId(const QString& name) {
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == name) return category.getCategoryId();
        }
        return -1;
    }
    static void settle() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }
    static QList<QAbstractButton*> rows(RulesSettings& settings) {
        QList<QAbstractButton*> result;
        for (QAbstractButton* row : settings.findChildren<QAbstractButton*>("ruleRow")) {
            if (row->isVisibleTo(&settings)) result.append(row);
        }
        return result;
    }
    static QWidget* firstCondition(RulesSettings& settings) {
        for (QWidget* condition : settings.findChildren<QWidget*>("ruleCondition")) {
            if (condition->isVisibleTo(&settings)) return condition;
        }
        return nullptr;
    }
    static void select(QComboBox* combo, const QString& text) {
        combo->setCurrentIndex(combo->findText(text));
    }
    int addRule(const QString& payee, const QString& category) {
        Rule rule;
        rule.categoryId = categoryId(category);
        rule.conditions = {{Field::Payee, Operator::Contains, payee}};
        return RuleDAO(1).addRule(rule);
    }

private slots:
    void initTestCase() {
        db.init();
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }
    void cleanupTestCase() {
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        Settings::instance().setCurrencySymbol(Settings::CurrencySymbol::EUR);
        Settings::instance().setDecimalSeparator(Settings::DecimalSeparator::Comma);
        Settings::instance().setThousandsSeparator(Settings::ThousandsSeparator::Point);
        Settings::instance().setCurrencySymbolPrefix(false);
        giroId = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        TransactionDAO transactions;
        transactions.addTransaction(Transaction(-1, giroId, "REWE Markt", 3240, QDate(2026, 10, 1), "", 0, "expense"));
        transactions.addTransaction(Transaction(-1, giroId, "Aldi", 1500, QDate(2026, 10, 2), "Einkauf bei REWE", 0, "expense"));
        transactions.addTransaction(Transaction(-1, giroId, "Stadtwerke", 8000, QDate(2026, 10, 3), "", 0, "expense"));
    }
    void cleanup() { db.close(); }

    void startsEmptyAndCreatesARule() {
        RulesSettings settings;
        settings.setUserId(1);
        QVERIFY(!settings.findChild<QLabel*>("noRulesLabel")->isHidden());
        QSignalSpy changed(&settings, &RulesSettings::rulesChanged);

        settings.findChild<QPushButton*>("addRuleButton")->click();
        QWidget* condition = firstCondition(settings);
        QVERIFY(condition);
        select(condition->findChild<QComboBox*>("conditionField"), "Payee or description");
        condition->findChild<QLineEdit*>("conditionText")->setText("rewe");
        select(settings.findChild<QComboBox*>("ruleCategoryCombo"), "Groceries");
        QCOMPARE(settings.findChild<QLabel*>("ruleEditorMatchCount")->text(), QString("Matches 2 existing transactions"));
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();

        QCOMPARE(changed.count(), 1);
        const QList<Rule> saved = RuleDAO(1).rules();
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().categoryId, categoryId("Groceries"));
        QCOMPARE(saved.first().conditions.first().field, Field::PayeeOrDescription);
        QCOMPARE(rows(settings).size(), 1);
        QCOMPARE(rows(settings).first()->accessibleName(), QString("Payee or description contains \"rewe\""));
        QCOMPARE(rows(settings).first()->findChild<QLabel*>("ruleMatchCount")->text(), QString("2 matches"));
    }

    void incompleteRulesAreNotSaved() {
        RulesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addRuleButton")->click();
        settings.findChild<QPushButton*>("saveRuleButton")->click();      // No text yet
        QVERIFY(!settings.findChild<QLabel*>("ruleError")->isHidden());
        QVERIFY(RuleDAO(1).rules().isEmpty());

        firstCondition(settings)->findChild<QToolButton*>("removeConditionButton")->click();
        settle();
        settings.findChild<QPushButton*>("saveRuleButton")->click();      // No conditions
        QCOMPARE(settings.findChild<QLabel*>("ruleError")->text(), QString("Add at least one condition."));
        QVERIFY(RuleDAO(1).rules().isEmpty());
    }

    void amountsUseTheMoneyFormatAndCanBeARange() {
        RulesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addRuleButton")->click();
        QWidget* condition = firstCondition(settings);
        select(condition->findChild<QComboBox*>("conditionField"), "Amount");
        select(condition->findChild<QComboBox*>("conditionOperator"), "is between");
        QVERIFY(condition->findChild<QLineEdit*>("conditionText")->isHidden());
        QVERIFY(!condition->findChild<QDoubleSpinBox*>("conditionAmountTo")->isHidden());
        condition->findChild<QDoubleSpinBox*>("conditionAmount")->setValue(40);
        condition->findChild<QDoubleSpinBox*>("conditionAmountTo")->setValue(120);
        select(settings.findChild<QComboBox*>("ruleCategoryCombo"), "Utilities");
        QCOMPARE(settings.findChild<QLabel*>("ruleEditorMatchCount")->text(), QString("Matches 1 existing transaction"));
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();

        const RuleCondition saved = RuleDAO(1).rules().first().conditions.first();
        QCOMPARE(saved.value, QString("4000"));
        QCOMPARE(saved.value2, QString("12000"));
        QCOMPARE(rows(settings).first()->accessibleName(), QString("Amount is between 40,00 € and 120,00 €"));
    }

    void rulesCanBeEditedReorderedAndDeleted() {
        const int rewe = addRule("REWE", "Groceries");
        const int netflix = addRule("Netflix", "Entertainment");
        RulesSettings settings;
        settings.setUserId(1);
        settings.confirm = [](const QString&, const QString&) { return true; };
        QCOMPARE(rows(settings).size(), 2);

        // ▼ on the first rule moves it below the second
        rows(settings).first()->findChild<QToolButton*>("moveRuleDownButton")->click();
        settle();
        QCOMPARE(RuleDAO(1).rules().first().id, netflix);
        QVERIFY(!rows(settings).first()->findChild<QToolButton*>("moveRuleUpButton")->isEnabled());

        // Edit the REWE rule (now second): any condition, another category
        rows(settings).last()->click();
        select(settings.findChild<QComboBox*>("ruleMatchCombo"), "any");
        select(settings.findChild<QComboBox*>("ruleCategoryCombo"), "Utilities");
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        const Rule edited = RuleDAO(1).rules().last();
        QCOMPARE(edited.id, rewe);
        QCOMPARE(edited.matchAll, false);
        QCOMPARE(edited.categoryId, categoryId("Utilities"));

        rows(settings).last()->click();
        settings.findChild<QPushButton*>("deleteRuleButton")->click();
        settle();
        QCOMPARE(RuleDAO(1).rules().size(), 1);
        QCOMPARE(rows(settings).size(), 1);
    }

    void aRuleCanStartFromADraft() {
        RulesSettings settings;
        settings.setUserId(1);
        Rule draft;
        draft.categoryId = categoryId("Utilities");
        draft.conditions = {{Field::Payee, Operator::Contains, "Stadtwerke"}};
        settings.createRule(draft);

        QWidget* condition = firstCondition(settings);
        QCOMPARE(condition->findChild<QLineEdit*>("conditionText")->text(), QString("Stadtwerke"));
        QCOMPARE(settings.findChild<QComboBox*>("ruleCategoryCombo")->currentText(), QString("Utilities"));
        QVERIFY(settings.findChild<QPushButton*>("deleteRuleButton")->isHidden());   // Not saved yet
        QCOMPARE(settings.findChild<QLabel*>("ruleEditorMatchCount")->text(), QString("Matches 1 existing transaction"));
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        QCOMPARE(RuleDAO(1).rules().size(), 1);
    }

    void describesEveryKindOfCondition() {
        RulesSettings settings;
        settings.setUserId(1);
        Rule rule;
        rule.matchAll = false;
        rule.conditions = {{Field::Description, Operator::StartsWith, "Gehalt"},
                           {Field::Type, Operator::Is, "income"},
                           {Field::Account, Operator::Is, QString::number(giroId)},
                           {Field::Amount, Operator::Is, "250000"}};
        QCOMPARE(settings.describe(rule),
                 QString("Description starts with \"Gehalt\" or it's income or account is Giro or amount is 2.500,00 €"));
    }
};

QTEST_MAIN(TestRulesSettings)
#include "test_rulessettings.moc"
