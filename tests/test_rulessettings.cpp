#include <QtTest>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>

#include "account.h"
#include "category.h"
#include "dialog/renamedpayeesdialog.h"
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

    void staysQuickWithManyTransactionsAndRules() {
        // Thousands of transactions and long rules, like a few years of bank exports
        QSqlDatabase database = DatabaseManager::instance().database();
        QVERIFY(database.transaction());
        TransactionDAO transactions;
        const QStringList payees = {"REWE Markt 4711", "Aldi Sued", "Stadtwerke Hannover", "Amazon EU", "DB Vertrieb GmbH",
                                    "Telekom Deutschland", "McFit", "Netflix", "Hetzner Online", "Trade Republic"};
        for (int i = 0; i < 5000; ++i) {
            transactions.addTransaction(Transaction(-1, giroId, payees[i % payees.size()], 1000 + i, QDate(2024, 1, 1).addDays(i % 900),
                                                    QString("Kartenzahlung %1 Lastschrift").arg(i), 0, "expense"));
        }
        QVERIFY(database.commit());
        for (int r = 0; r < 15; ++r) {
            Rule rule;
            rule.categoryId = categoryId("Groceries");
            rule.matchAll = false;
            for (int c = 0; c < 5; ++c) {
                rule.conditions.append({Field::PayeeOrDescription, Operator::Contains, QString("Shop%1-%2").arg(r).arg(c)});
            }
            QVERIFY(RuleDAO(1).addRule(rule) > 0);
        }

        QElapsedTimer timer;
        timer.start();
        RulesSettings settings;
        settings.setUserId(1);
        const qint64 loading = timer.restart();
        rows(settings).first()->click();                 // Opening a rule
        const qint64 opening = timer.restart();
        QTest::keyClicks(firstCondition(settings)->findChild<QLineEdit*>("conditionText"), "abcdefghij");
        const qint64 typing = timer.restart();
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        const qint64 saving = timer.restart();
        // Loading counts every rule's matches once. Opening a rule and saving it mustn't count them
        // all again (they took longer than loading before), whatever the build's speed.
        const QByteArray times = QString("loading %1 ms, opening %2 ms, typing 10 keys %3 ms, saving %4 ms")
                                     .arg(loading).arg(opening).arg(typing).arg(saving).toUtf8();
        QVERIFY2(opening * 3 < loading, times.constData());
        QVERIFY2(saving * 3 < loading, times.constData());
    }

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

    void removingAConditionShowsAnIcon() {
        // An icon, not a ✕ in text (Windows searched its fonts for that for over half a second).
        // A missing SVG gives a blank icon, so look at the pixels.
        RulesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addRuleButton")->click();
        auto* remove = firstCondition(settings)->findChild<QToolButton*>("removeConditionButton");
        QCOMPARE(remove->toolButtonStyle(), Qt::ToolButtonIconOnly);
        const QImage image = remove->icon().pixmap(remove->iconSize()).toImage();
        bool drawn = false;
        for (int y = 0; y < image.height() && !drawn; ++y) {
            for (int x = 0; x < image.width() && !drawn; ++x) drawn = qAlpha(image.pixel(x, y)) > 0;
        }
        QVERIFY2(drawn, "The x icon is blank: is assets/icons/x.svg in the resources?");
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

    void namedRulesShowTheirNameAndLongRulesFitTheList() {
        Rule rule;
        rule.categoryId = categoryId("Groceries");
        rule.matchAll = false;
        for (const char* shop : {"Aldi", "Rewe", "Lidl", "Edeka", "Netto", "Penny", "Wucherpfennig", "E-Center",
                                 "Kaufland", "Real", "Globus", "Famila"}) {
            rule.conditions.append({Field::PayeeOrDescription, Operator::Contains, shop});
        }
        QVERIFY(RuleDAO(1).addRule(rule) > 0);
        RulesSettings settings;
        settings.resize(700, 500);
        settings.show();
        settings.setUserId(1);
        QString asked;
        settings.confirm = [&](const QString&, const QString& text) { asked = text; return false; };
        QCoreApplication::processEvents();               // Rows added to a shown page appear on the next turn
        QCOMPARE(rows(settings).size(), 1);
        const QString conditions = rows(settings).first()->accessibleName();
        QVERIFY(conditions.startsWith("Payee or description contains \"Aldi\""));   // Unnamed: its conditions

        // However long the rule, the list doesn't scroll sideways
        auto* scroll = settings.findChild<QScrollArea*>();
        QVERIFY(scroll);
        const auto noSideways = [scroll]() {
            QCoreApplication::processEvents();
            return scroll->horizontalScrollBar()->maximum() == 0;
        };
        QVERIFY(noSideways());

        rows(settings).first()->click();
        auto* nameEdit = settings.findChild<QLineEdit*>("ruleNameEdit");
        QVERIFY(nameEdit);
        nameEdit->setText(" Supermarkets ");
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        QCoreApplication::processEvents();
        QCOMPARE(RuleDAO(1).rules().first().name, QString("Supermarkets"));
        QCOMPARE(rows(settings).size(), 1);
        QAbstractButton* row = rows(settings).first();
        QCOMPARE(row->accessibleName(), QString("Supermarkets"));
        QCOMPARE(row->accessibleDescription(), conditions);              // Still there, below the name
        QCOMPARE(row->toolTip(), conditions);
        QVERIFY(noSideways());

        // Opening it shows the name; deleting asks by name
        row->click();
        QCOMPARE(nameEdit->text(), QString("Supermarkets"));
        auto* deleteButton = settings.findChild<QPushButton*>("deleteRuleButton");
        QVERIFY(deleteButton);
        deleteButton->click();
        QVERIFY(asked.startsWith("Delete the rule \"Supermarkets\"?"));
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

    void rulesCanRenameWithOrWithoutACategory() {
        RulesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addRuleButton")->click();
        firstCondition(settings)->findChild<QLineEdit*>("conditionText")->setText("REWE");
        auto* setsCategory = settings.findChild<QCheckBox*>("ruleSetsCategoryCheck");
        auto* renames = settings.findChild<QCheckBox*>("ruleRenamesCheck");
        auto* newPayee = settings.findChild<QLineEdit*>("ruleNewPayeeEdit");
        auto* error = settings.findChild<QLabel*>("ruleError");
        QVERIFY(setsCategory->isChecked());              // A new rule starts with the category ticked
        QVERIFY(!renames->isChecked());

        // Nothing ticked, or rename ticked without a name: not saved
        setsCategory->setChecked(false);
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        QVERIFY(error->text().startsWith("Tick what the rule does"));
        renames->setChecked(true);
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        QCOMPARE(error->text(), QString("Enter the new name for the payee."));
        QVERIFY(RuleDAO(1).rules().isEmpty());

        // Only renaming
        newPayee->setText("  Rewe ");
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        const Rule saved = RuleDAO(1).rules().first();
        QCOMPARE(saved.newPayee, QString("Rewe"));
        QCOMPARE(saved.categoryId, -1);
        QAbstractButton* row = rows(settings).first();
        QCOMPARE(row->findChild<QLabel*>("ruleRename")->text(), QString("→ \"Rewe\""));

        // Reopened: rename ticked, category not; then both
        row->click();
        QVERIFY(!setsCategory->isChecked());
        QVERIFY(renames->isChecked());
        QCOMPARE(newPayee->text(), QString("Rewe"));
        setsCategory->setChecked(true);
        select(settings.findChild<QComboBox*>("ruleCategoryCombo"), "Groceries");
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        QCOMPARE(RuleDAO(1).rules().first().categoryId, categoryId("Groceries"));
        QCOMPARE(RuleDAO(1).rules().first().newPayee, QString("Rewe"));
    }

    void changingARulesRenameOffersToUpdateItsNames() {
        TransactionDAO transactions;
        transactions.addTransaction(Transaction(-1, giroId, "REWE City", 990, QDate(2026, 10, 4), "", 0, "expense"));
        Rule rule;
        rule.newPayee = "Rewe";
        rule.conditions = {{Field::Payee, Operator::Contains, "REWE"}};
        rule.id = RuleDAO(1).addRule(rule);
        QVERIFY(Rules::apply(Rules::changesFor({rule}, transactions.getTransactionsForAccount(giroId), false, {})));
        QCOMPARE(transactions.getTransactionsRenamedByRule(rule.id).size(), 2);
        const auto payees = [&]() {
            QStringList result;
            for (const Transaction& transaction : transactions.getTransactionsForAccount(giroId)) {
                result.append(transaction.getPayee());
            }
            result.sort();
            return result;
        };

        RulesSettings settings;
        settings.setUserId(1);
        settings.confirm = [](const QString&, const QString&) { return true; };
        QSignalSpy changed(&settings, &RulesSettings::transactionsChanged);
        QString shownIntro;
        std::function<void(RenamedPayeesDialog&)> answer;
        settings.reviewRenamedPayees = [&](RenamedPayeesDialog& dialog) {
            shownIntro = dialog.findChild<QLabel*>("renamedPayeesIntro")->text();
            answer(dialog);
        };
        auto* renames = settings.findChild<QCheckBox*>("ruleRenamesCheck");
        auto* newPayee = settings.findChild<QLineEdit*>("ruleNewPayeeEdit");

        // A new name: both get it, the bank's names stay
        answer = [](RenamedPayeesDialog& dialog) {
            dialog.findChild<QPushButton*>("renamedPayeesApplyButton")->click();
        };
        rows(settings).first()->click();
        newPayee->setText("Rewe Markt");
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        QCOMPARE(shownIntro, QString("This rule renamed 2 transactions to \"Rewe\". Give them the new name \"Rewe Markt\" too?"));
        QCOMPARE(payees(), (QStringList{"Aldi", "Rewe Markt", "Rewe Markt", "Stadtwerke"}));
        QCOMPARE(transactions.getTransactionsRenamedByRule(rule.id).first().getOriginalPayee(), QString("REWE City"));
        QCOMPARE(changed.count(), 1);

        // Rename turned off, names kept: nothing changes
        answer = [](RenamedPayeesDialog& dialog) { dialog.findChild<QPushButton*>("keepNamesButton")->click(); };
        rows(settings).first()->click();
        renames->setChecked(false);
        settings.findChild<QCheckBox*>("ruleSetsCategoryCheck")->setChecked(true);
        settings.findChild<QPushButton*>("saveRuleButton")->click();
        settle();
        QVERIFY(shownIntro.startsWith("This rule renamed 2 transactions to \"Rewe Markt\". Restore"));
        QCOMPARE(payees(), (QStringList{"Aldi", "Rewe Markt", "Rewe Markt", "Stadtwerke"}));
        QCOMPARE(changed.count(), 1);

        // Deleted: still offered; only the ticked one gets its name back (the newest is listed first)
        shownIntro.clear();
        answer = [](RenamedPayeesDialog& dialog) {
            dialog.findChildren<QCheckBox*>("renamedPayeeCheck").first()->setChecked(false);
            QCOMPARE(dialog.findChild<QPushButton*>("renamedPayeesApplyButton")->text(), QString("Restore 1"));
            dialog.findChild<QPushButton*>("renamedPayeesApplyButton")->click();
        };
        rows(settings).first()->click();
        settings.findChild<QPushButton*>("deleteRuleButton")->click();
        settle();
        QCOMPARE(shownIntro, QString("This rule renamed 2 transactions. Restore their original names, or keep the names they have now?"));
        QCOMPARE(payees(), (QStringList{"Aldi", "REWE Markt", "Rewe Markt", "Stadtwerke"}));
        for (const Transaction& transaction : transactions.getTransactionsForAccount(giroId)) {
            if (transaction.getPayee() == "REWE Markt") QVERIFY(transaction.getOriginalPayee().isEmpty());
            QCOMPARE(transaction.getRenamedByRuleId(), -1);   // The rule is gone
        }
        QCOMPARE(changed.count(), 2);
    }

    void openingARuleKeepsTheListWhereItIs() {
        for (int i = 0; i < 25; ++i) {
            QVERIFY(addRule(QString("Shop %1").arg(i), "Groceries") > 0);
        }
        RulesSettings settings;
        settings.setUserId(1);
        settings.resize(700, 400);
        settings.show();
        settings.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&settings));
        auto* scrollArea = settings.findChild<QScrollArea*>();
        auto* scrollBar = scrollArea->verticalScrollBar();
        QVERIFY(scrollBar->maximum() > 0);
        scrollBar->setValue(scrollBar->maximum() / 2);
        const int scrolled = scrollBar->value();

        QAbstractButton* clicked = nullptr;
        for (QAbstractButton* candidate : rows(settings)) {
            const QPoint top = candidate->mapTo(scrollArea->viewport(), QPoint(0, 0));
            if (top.y() > 40 && top.y() < scrollArea->viewport()->height() - 80) {
                clicked = candidate;
                break;
            }
        }
        QVERIFY(clicked);
        QTest::mouseClick(clicked, Qt::LeftButton, {}, QPoint(20, clicked->height() / 2));
        settle();
        QVERIFY(settings.findChild<QFrame*>("ruleEditor")->isVisible());
        QCOMPARE(scrollBar->value(), scrolled);
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
