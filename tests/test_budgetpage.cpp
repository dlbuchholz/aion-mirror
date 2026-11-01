#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>

#include "account.h"
#include "budget.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/budgeteditor.h"
#include "widgets/budgetpage.h"

class TestBudgetPage : public QObject {
    Q_OBJECT

    TestDbFixture db;
    int accountId = -1;
    int groceries = 0;
    int rent = 0;
    QDate thisMonth;

    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    void spend(Cents cents, int category) {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, accountId, "Shop", cents, thisMonth, "", category, "expense")) > 0);
    }

    static QAbstractButton* row(BudgetPage& page, const QString& category) {
        for (QAbstractButton* button : page.findChildren<QAbstractButton*>("budgetRow")) {
            if (button->accessibleName() == category) return button;
        }
        return nullptr;
    }

    static QString money(Cents cents) { return Settings::instance().formatCurrency(cents); }
    static QString label(BudgetPage& page, const char* name) { return page.findChild<QLabel*>(name)->text(); }

    static Cents limitIn(const QDate& month, int category) {
        for (const MonthBudget& budget : BudgetDAO().budgetsForMonth(1, month)) {
            if (budget.categoryId == category) return budget.limitCents;
        }
        return 0;
    }

    static bool editorSays(BudgetEditor& editor, const QString& message) {
        for (QLabel* text : editor.findChildren<QLabel*>()) {
            if (text->text() == message) return true;
        }
        return false;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        accountId = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        groceries = categoryId("Groceries");
        rent = categoryId("Rent");
        const QDate today = QDate::currentDate();
        thisMonth = QDate(today.year(), today.month(), 1);
    }
    void cleanup() { db.close(); }

    void showsBudgetsWithTotals() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        spend(31200, groceries);
        BudgetPage page;
        page.setUserId(1);

        QAbstractButton* groceriesRow = row(page, "Groceries");
        QVERIFY(groceriesRow);
        QCOMPARE(groceriesRow->accessibleDescription(), QString("%1 of %2 · %3 left").arg(money(31200), money(40000), money(8800)));
        QCOMPARE(label(page, "budgetedTotal"), money(40000));
        QCOMPARE(label(page, "spentTotal"), money(31200));
        QCOMPARE(label(page, "leftTotal"), money(8800));
        QVERIFY(!page.findChild<QLabel*>("budgetEmptyLabel")->isVisibleTo(&page));
        QVERIFY(!page.findChild<QPushButton*>("thisMonthButton")->isVisibleTo(&page));
    }

    void overspendingIsShownAsOver() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 10000));
        spend(12500, groceries);
        BudgetPage page;
        page.setUserId(1);
        QVERIFY(row(page, "Groceries")->accessibleDescription().endsWith(QString("%1 over").arg(money(2500))));
        QCOMPARE(label(page, "leftTotal"), money(2500));
        QVERIFY(page.findChildren<QLabel*>("tileCaption").last()->text() == "Over budget");
    }

    void emptyMonthExplainsWhatToDo() {
        BudgetPage page;
        page.setUserId(1);
        QVERIFY(page.findChildren<QAbstractButton*>("budgetRow").isEmpty());
        QVERIFY(page.findChild<QLabel*>("budgetEmptyLabel")->isVisibleTo(&page));
        QVERIFY(!page.findChild<QLabel*>("unbudgetedTitle")->isVisibleTo(&page));
    }

    void addingABudgetForEveryMonth() {
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy changed(&page, &BudgetPage::budgetsChanged);

        page.findChild<QPushButton*>("addBudgetButton")->click();
        auto* editor = page.findChild<BudgetEditor*>();
        QVERIFY(editor->isVisibleTo(&page));
        auto* combo = editor->findChild<QComboBox*>("budgetCategoryCombo");
        QVERIFY(combo->findText("Salary") < 0);                // Only expense categories
        combo->setCurrentIndex(combo->findData(rent));
        editor->findChild<QDoubleSpinBox*>("budgetLimitEdit")->setValue(900.0);
        QVERIFY(editor->findChild<QRadioButton*>("everyMonthButton")->isChecked());   // The default
        editor->findChild<QPushButton*>("budgetSaveButton")->click();

        QVERIFY(!editor->isVisibleTo(&page));
        QCOMPARE(changed.count(), 1);
        QVERIFY(row(page, "Rent"));
        QCOMPARE(limitIn(thisMonth.addMonths(3), rent), Cents(90000));
    }

    void changingOnlyThisMonth() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        row(page, "Groceries")->click();

        auto* editor = page.findChild<BudgetEditor*>();
        QCOMPARE(editor->editedCategoryId(), groceries);
        QVERIFY(!editor->findChild<QComboBox*>("budgetCategoryCombo")->isVisibleTo(editor));
        auto* limit = editor->findChild<QDoubleSpinBox*>("budgetLimitEdit");
        QCOMPARE(limit->value(), 400.0);
        limit->setValue(550.0);
        editor->findChild<QRadioButton*>("onlyThisMonthButton")->setChecked(true);
        editor->findChild<QPushButton*>("budgetSaveButton")->click();

        QCOMPARE(limitIn(thisMonth, groceries), Cents(55000));
        QCOMPARE(limitIn(thisMonth.addMonths(1), groceries), Cents(40000));
        QVERIFY(row(page, "Groceries")->toolTip().contains("only"));
    }

    void removingOnlyThisMonth() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        row(page, "Groceries")->click();

        auto* remove = page.findChild<QPushButton*>("budgetRemoveButton");
        QVERIFY(remove->menu());                               // Regular limits offer both ways
        const QList<QAction*> actions = remove->menu()->actions();
        QCOMPARE(actions.size(), 2);
        QVERIFY(actions[1]->text().startsWith("Only in"));
        actions[1]->trigger();

        QVERIFY(!row(page, "Groceries"));
        QCOMPARE(limitIn(thisMonth.addMonths(1), groceries), Cents(40000));
    }

    void removingASingleMonthNeedsNoChoice() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        row(page, "Groceries")->click();
        auto* remove = page.findChild<QPushButton*>("budgetRemoveButton");
        QVERIFY(!remove->menu());
        remove->click();
        QVERIFY(!row(page, "Groceries"));
    }

    void unbudgetedSpendingSuggestsALimit() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        spend(4640, rent);
        spend(1000, 0);                                        // No category: can't get a budget
        BudgetPage page;
        page.setUserId(1);
        QVERIFY(page.findChild<QLabel*>("unbudgetedTitle")->isVisibleTo(&page));
        const QList<QToolButton*> addButtons = page.findChildren<QToolButton*>("addBudgetForCategory");
        QCOMPARE(addButtons.size(), 1);

        addButtons.first()->click();
        auto* editor = page.findChild<BudgetEditor*>();
        QCOMPARE(editor->findChild<QComboBox*>("budgetCategoryCombo")->currentData().toInt(), rent);
        QCOMPARE(editor->findChild<QDoubleSpinBox*>("budgetLimitEdit")->value(), 50.0);   // Rounded up
        QVERIFY(editor->findChild<QComboBox*>("budgetCategoryCombo")->findData(groceries) < 0);  // Already budgeted
    }

    void navigatesBetweenMonths() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        page.findChild<QToolButton*>("previousMonthButton")->click();
        QCOMPARE(page.month(), thisMonth.addMonths(-1));
        QVERIFY(!row(page, "Groceries"));
        QCOMPARE(label(page, "monthLabel"), QLocale(QLocale::English).toString(thisMonth.addMonths(-1), "MMMM yyyy"));

        auto* back = page.findChild<QPushButton*>("thisMonthButton");
        QVERIFY(back->isVisibleTo(&page));
        back->click();
        QCOMPARE(page.month(), thisMonth);
        QVERIFY(row(page, "Groceries"));
    }

    void enterOpensABudgetRow() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, rent, thisMonth, 90000));
        BudgetPage page;
        page.setUserId(1);
        QTest::keyClick(row(page, "Rent"), Qt::Key_Return);
        QVERIFY(page.findChild<BudgetEditor*>()->isVisibleTo(&page));
    }

    void aBudgetThatCantBeSavedKeepsTheEditorOpen() {
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy changed(&page, &BudgetPage::budgetsChanged);
        page.findChild<QPushButton*>("addBudgetButton")->click();
        auto* editor = page.findChild<BudgetEditor*>();
        auto* combo = editor->findChild<QComboBox*>("budgetCategoryCombo");
        combo->setCurrentIndex(combo->findData(rent));
        editor->findChild<QDoubleSpinBox*>("budgetLimitEdit")->setValue(900.0);
        QVERIFY(TestDbFixture::refuse("INSERT", "budgets"));
        editor->findChild<QPushButton*>("budgetSaveButton")->click();
        QVERIFY(editor->isVisibleTo(&page));
        QCOMPARE(changed.count(), 0);
    }

    void aBudgetThatCantBeRemovedStays() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy changed(&page, &BudgetPage::budgetsChanged);
        row(page, "Groceries")->click();
        QVERIFY(TestDbFixture::refuse("DELETE", "budgets"));
        page.findChild<QPushButton*>("budgetRemoveButton")->click();
        auto* editor = page.findChild<BudgetEditor*>();
        QVERIFY(editor->isVisibleTo(&page));
        QVERIFY(editorSays(*editor, "The budget could not be removed."));
        QCOMPARE(limitIn(thisMonth, groceries), Cents(40000));
        QCOMPARE(changed.count(), 0);
    }

    void aNewBudgetWithNoCategoriesLeftSaysSo() {
        BudgetEditor editor;
        editor.newBudget(thisMonth, {});
        QVERIFY(editorSays(editor, "Every expense category already has a budget this month. Add categories under Settings → Categories."));
    }

    void editorKeys() {
        BudgetEditor editor;
        QSignalSpy closed(&editor, &BudgetEditor::closed);
        QSignalSpy save(&editor, &BudgetEditor::saveRequested);
        QTest::keyClick(&editor, Qt::Key_Return);        // No category chosen: an error, not a save
        QCOMPARE(save.count(), 0);
        QTest::keyClick(&editor, Qt::Key_A);             // Other keys do nothing
        QCOMPARE(closed.count(), 0);
        QTest::keyClick(&editor, Qt::Key_Escape);
        QCOMPARE(closed.count(), 1);
    }

    void invalidLimitShowsAnError() {
        BudgetPage page;
        page.setUserId(1);
        page.findChild<QPushButton*>("addBudgetButton")->click();
        auto* editor = page.findChild<BudgetEditor*>();
        editor->findChild<QDoubleSpinBox*>("budgetLimitEdit")->setValue(0.0);
        editor->findChild<QPushButton*>("budgetSaveButton")->click();
        QVERIFY(editor->isVisibleTo(&page));
        QVERIFY(BudgetDAO().budgetsForMonth(1, thisMonth).isEmpty());
    }
};

QTEST_MAIN(TestBudgetPage)
#include "test_budgetpage.moc"
