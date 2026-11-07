#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>

#include "account.h"
#include "budget.h"
#include "categorycolors.h"
#include "settings.h"
#include "theme.h"
#include "tests/test_db_fixture.h"
#include "widgets/budgetdetail.h"
#include "widgets/budgeteditor.h"
#include "widgets/budgetpage.h"
#include "widgets/periodpicker.h"

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

    // Unfolds the budget and opens it in the editor
    static void edit(BudgetPage& page, const QString& category) {
        QAbstractButton* budget = row(page, category);
        QVERIFY(budget);
        budget->click();
        auto* detail = page.findChild<BudgetDetail*>();
        QVERIFY(detail);
        detail->findChild<QPushButton*>("budgetDetailEdit")->click();
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
    void initTestCase() {
        db.init();
        // The app's own look on every platform. Without it the system style's light grey sits under
        // the dark theme's faint white highlights, and on some (windows11) two of them round to one colour.
        Theme::applyTheme(Theme::Type::Dark);
    }
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
        QCOMPARE(groceriesRow->accessibleDescription(), QString("1 transaction · %1 of %2 · %3 left").arg(money(31200), money(40000), money(8800)));
        QCOMPARE(label(page, "budgetedTotal"), money(40000));
        QCOMPARE(label(page, "spentTotal"), money(31200));
        QCOMPARE(label(page, "leftTotal"), money(8800));
        QVERIFY(!page.findChild<QLabel*>("budgetEmptyLabel")->isVisibleTo(&page));
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
        edit(page, "Groceries");

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
        edit(page, "Groceries");

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
        edit(page, "Groceries");
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

    void budgetsLeadToTheirTransactions() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        QVERIFY(BudgetDAO().setLimitForMonth(1, rent, thisMonth.addMonths(-1), 90000));   // Nothing spent then
        spend(2500, groceries);
        spend(1200, groceries);
        spend(4640, rent);
        spend(1000, 0);
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy requested(&page, &BudgetPage::transactionsRequested);
        const QDate lastDay = thisMonth.addMonths(1).addDays(-1);
        const auto lastRequest = [&requested]() {
            const QList<QVariant> request = requested.last();
            return std::tuple{request[0].toInt(), request[1].toDate(), request[2].toDate()};
        };

        // From the editor of a budget, not of a new one
        auto* editor = page.findChild<BudgetEditor*>();
        auto* link = editor->findChild<QToolButton*>("budgetTransactionsLink");
        page.findChild<QPushButton*>("addBudgetButton")->click();
        QVERIFY(link->isHidden());
        edit(page, "Groceries");
        QVERIFY(!link->isHidden());
        const QString monthName = QLocale(QLocale::English).toString(thisMonth, "MMMM");
        QCOMPARE(link->text(), QString("Show the 2 transactions in %1 ›").arg(monthName));
        QVERIFY(link->isEnabled());
        QVERIFY(row(page, "Groceries")->accessibleDescription().startsWith("2 transactions ·"));
        link->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(lastRequest(), std::tuple(groceries, thisMonth, lastDay));

        // From a budget row's menu
        QStringList entries;
        QTimer::singleShot(0, &page, [&entries]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            const QList<QAction*> actions = menu->actions();
            menu->close();                               // First, so a failing check can't leave it open
            for (QAction* action : actions) entries << action->text();
            if (!actions.isEmpty()) actions.first()->trigger();
        });
        emit row(page, "Groceries")->customContextMenuRequested(QPoint(4, 4));
        QCOMPARE(entries, (QStringList{"Show 2 transactions", "Edit budget…"}));
        QCOMPARE(requested.size(), 2);
        QCOMPARE(lastRequest(), std::tuple(groceries, thisMonth, lastDay));

        // From spending without a budget, also without a category; adding a budget stays its own
        QAbstractButton* rentRow = nullptr;
        QAbstractButton* noCategoryRow = nullptr;
        for (QAbstractButton* unbudgeted : page.findChildren<QAbstractButton*>("unbudgetedRow")) {
            (unbudgeted->accessibleName() == "Rent" ? rentRow : noCategoryRow) = unbudgeted;
        }
        QVERIFY(rentRow && noCategoryRow);
        QCOMPARE(rentRow->findChild<QLabel*>("unbudgetedCount")->text(), QString("1 transaction"));
        QCOMPARE(rentRow->toolTip(), QString("Show the 1 transaction in %1").arg(monthName));
        rentRow->click();
        QCOMPARE(lastRequest(), std::tuple(rent, thisMonth, lastDay));
        noCategoryRow->click();
        QCOMPARE(lastRequest(), std::tuple(0, thisMonth, lastDay));
        QCOMPARE(requested.size(), 4);
        rentRow->findChild<QToolButton*>("addBudgetForCategory")->click();
        QCOMPARE(requested.size(), 4);

        // Another month: its days, and nothing to show for a budget without transactions
        page.findChild<QToolButton*>("previousPeriod")->click();
        emit editor->transactionsRequested(groceries);
        QCOMPARE(lastRequest(), std::tuple(groceries, thisMonth.addMonths(-1), thisMonth.addDays(-1)));
        edit(page, "Rent");
        QCOMPARE(link->text(), QString("No transactions in %1 yet").arg(QLocale(QLocale::English).toString(thisMonth.addMonths(-1), "MMMM")));
        QVERIFY(!link->isEnabled());
        bool showEnabled = true;
        QTimer::singleShot(0, &page, [&showEnabled]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) return;
            menu->close();
            showEnabled = menu->actions().first()->isEnabled();
        });
        emit row(page, "Rent")->customContextMenuRequested(QPoint(4, 4));
        QVERIFY(!showEnabled);

        MonthBudget single;
        single.categoryId = groceries;
        single.limitCents = 100;
        single.transactionCount = 1;
        editor->editBudget(thisMonth, single);
        QCOMPARE(link->text(), QString("Show the transaction in %1 ›").arg(monthName));
    }

    void navigatesBetweenMonths() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        auto* name = page.findChild<QToolButton*>("periodName");
        const auto monthName = [](const QDate& month) { return QLocale(QLocale::English).toString(month, "MMMM yyyy"); };
        QCOMPARE(name->text(), monthName(thisMonth));
        page.findChild<QToolButton*>("previousPeriod")->click();
        QCOMPARE(page.month(), thisMonth.addMonths(-1));
        QVERIFY(!row(page, "Groceries"));
        QCOMPARE(name->text(), monthName(thisMonth.addMonths(-1)));

        // The name's menu: this month and any month, no longer periods; later months too, for planning
        name->click();
        auto* menu = page.findChild<PeriodMenu*>();
        QVERIFY(menu->isVisible());
        QStringList shortcuts;
        QAbstractButton* back = nullptr;
        for (QPushButton* shortcut : menu->findChildren<QPushButton*>()) {
            if (shortcut->isVisibleTo(menu)) shortcuts.append(shortcut->text());
            if (shortcut->text() == "This month") back = shortcut;
        }
        QCOMPARE(shortcuts, QStringList({"This month", "Last month"}));
        QVERIFY(menu->findChild<QToolButton*>("nextYear")->isEnabled());
        QVERIFY(menu->findChild<QToolButton*>("month12")->isEnabled());
        back->click();
        QCOMPARE(page.month(), thisMonth);
        QVERIFY(row(page, "Groceries"));

        // And on into the months to come
        auto* next = page.findChild<QToolButton*>("nextPeriod");
        QVERIFY(next->isEnabled());
        next->click();
        QCOMPARE(page.month(), thisMonth.addMonths(1));
        QCOMPARE(name->text(), monthName(thisMonth.addMonths(1)));
    }

    void enterUnfoldsABudgetRow() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, rent, thisMonth, 90000));
        BudgetPage page;
        page.setUserId(1);
        QTest::keyClick(row(page, "Rent"), Qt::Key_Return);
        QVERIFY(page.findChild<BudgetDetail*>());
        QVERIFY(page.findChild<BudgetEditor*>()->isHidden());
    }

    void rowsUnfoldOneAtATime() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        QVERIFY(BudgetDAO().setLimitForMonth(1, rent, thisMonth, 90000));
        BudgetPage page;
        page.setUserId(1);
        QVERIFY(!page.findChild<BudgetDetail*>());
        auto* layout = page.findChild<QWidget*>("budgetContent")->layout()->itemAt(0)->layout();
        const auto detailFollows = [&](const QString& category) {
            const int index = layout->indexOf(row(page, category));
            QWidget* next = layout->itemAt(index + 1) ? layout->itemAt(index + 1)->widget() : nullptr;
            return index >= 0 && qobject_cast<BudgetDetail*>(next) != nullptr;
        };

        row(page, "Groceries")->click();
        QVERIFY(detailFollows("Groceries"));
        row(page, "Rent")->click();
        QCOMPARE(page.findChildren<BudgetDetail*>().size(), 1);
        QVERIFY(detailFollows("Rent"));
        page.reload();                                   // New transactions, say
        QVERIFY(detailFollows("Rent"));
        row(page, "Rent")->click();                      // Folds again
        QVERIFY(!page.findChild<BudgetDetail*>());

        // A month without that budget folds it, and it stays folded on the way back
        row(page, "Rent")->click();
        page.findChild<QToolButton*>("nextPeriod")->click();
        QVERIFY(!page.findChild<BudgetDetail*>());
        page.findChild<QToolButton*>("previousPeriod")->click();
        QVERIFY(!page.findChild<BudgetDetail*>());
        // ... while a budget in both months stays unfolded
        row(page, "Groceries")->click();
        page.findChild<QToolButton*>("nextPeriod")->click();
        QVERIFY(detailFollows("Groceries"));
    }

    void anUnfoldedBudgetLeadsOn() {
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth.addMonths(-1), 40000));
        spend(2500, groceries);
        Transaction earlier(-1, accountId, "Market", 9900, thisMonth.addMonths(-1), "", groceries, "expense");
        QVERIFY(TransactionDAO().addTransaction(earlier) > 0);
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy one(&page, &BudgetPage::transactionRequested);
        QSignalSpy all(&page, &BudgetPage::transactionsRequested);
        row(page, "Groceries")->click();
        auto* detail = page.findChild<BudgetDetail*>();
        QCOMPARE(detail->findChild<QLabel*>("lastMonthValue")->text(), QString("%1 of %2").arg(money(9900), money(40000)));

        const QList<HoverRow*> listed = detail->findChildren<HoverRow*>("budgetDetailTransaction");
        QCOMPARE(listed.size(), 1);
        listed.first()->click();
        QCOMPARE(one.size(), 1);
        QCOMPARE(one.first().first().toInt(), BudgetDAO().transactions(1, groceries, thisMonth).first().transactionId);
        detail->findChild<QToolButton*>("budgetDetailTransactions")->click();
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().first().toInt(), groceries);
        detail->findChild<QPushButton*>("budgetDetailEdit")->click();
        QCOMPARE(page.findChild<BudgetEditor*>()->editedCategoryId(), groceries);
        QVERIFY(page.findChild<BudgetDetail*>());        // Stays unfolded next to the editor
    }

    // November 2026 with six purchases, 312,00 € of 400,00 €
    static QList<BudgetTransaction> november() {
        const QList<std::pair<int, Cents>> days = {{1, 6420}, {4, 3890}, {7, 7140}, {9, 2210}, {11, 4130}, {17, 7410}};
        QList<BudgetTransaction> list;
        for (int i = 0; i < days.size(); ++i) {
            list.append({i + 1, QDate(2026, 11, days[i].first), QString("P%1").arg(i + 1), days[i].second});
        }
        return list;
    }
    static MonthBudget groceriesBudget(Cents spent) {
        MonthBudget budget;
        budget.categoryId = 7;
        budget.categoryName = "Groceries";
        budget.limitCents = 40000;
        budget.spentCents = spent;
        return budget;
    }
    static QString text(QWidget& widget, const char* name) {
        auto* found = widget.findChild<QLabel*>(name);
        return found ? found->text() : QString("(none)");
    }
    static QColor color(QWidget& widget, const char* name) {
        return widget.findChild<QLabel*>(name)->palette().color(QPalette::WindowText);
    }

    void anUnfoldedBudgetShowsHowTheMonthIsGoing() {
        const QDate month(2026, 11, 1);
        BudgetDetail detail(groceriesBudget(31200), month, november(), 35000, 40000, QDate(2026, 11, 18));
        QCOMPARE(text(detail, "perDayValueCaption"), QString("Per day so far"));
        QCOMPARE(text(detail, "perDayValue"), Settings::instance().formatSpending(31200 / 18));
        QCOMPARE(text(detail, "paceValue"), Settings::instance().formatSpending(31200 * 30 / 18));
        QCOMPARE(color(detail, "paceValue"), Theme::colors().danger);           // 520,00 € is over the limit
        QCOMPARE(text(detail, "leftPerDayValue"), money(8800 / 13));            // The 18th to the 30th
        QCOMPARE(text(detail, "lastMonthValueCaption"), QString("October"));
        QCOMPARE(text(detail, "lastMonthValue"), QString("%1 of %2").arg(money(35000), money(40000)));
        QVERIFY(color(detail, "lastMonthValue") != Theme::colors().danger);

        // The newest four, newest first, each with its amount
        const QList<HoverRow*> listed = detail.findChildren<HoverRow*>("budgetDetailTransaction");
        QCOMPARE(listed.size(), BudgetDetail::ShownTransactions);
        QCOMPARE(listed.first()->accessibleName(), QString("P6"));
        QCOMPARE(listed.last()->accessibleName(), QString("P3"));
        QCOMPARE(listed.first()->findChild<QLabel*>("budgetDetailAmount")->text(), Settings::instance().formatCurrency(-7410, true));
        QCOMPARE(detail.findChild<QToolButton*>("budgetDetailTransactions")->text(), QString("Show all 6 transactions ›"));
        QVERIFY(detail.findChild<QToolButton*>("budgetDetailTransactions")->property("link").toBool());   // Looks like every link
        QVERIFY(!detail.findChild<QLabel*>("budgetDetailEmpty"));
        QSignalSpy one(&detail, &BudgetDetail::transactionRequested);
        listed.last()->click();
        QCOMPARE(one.last().first().toInt(), 3);
        emit detail.findChild<BudgetMonthChart*>()->transactionClicked(5);
        QCOMPARE(one.last().first().toInt(), 5);

        // Over the limit, and last month over its own
        BudgetDetail over(groceriesBudget(45000), month, november(), 45000, 40000, QDate(2026, 11, 18));
        QCOMPARE(text(over, "leftPerDayValue"), QString("Nothing left"));
        QCOMPARE(color(over, "leftPerDayValue"), Theme::colors().danger);
        QCOMPARE(color(over, "lastMonthValue"), Theme::colors().danger);
        BudgetDetail usedUp(groceriesBudget(40000), month, november(), 0, 0, QDate(2026, 11, 18));
        QCOMPARE(text(usedUp, "leftPerDayValue"), QString("Nothing left"));
    }

    void pastAndFutureMonthsHaveNoPace() {
        const QDate month(2026, 11, 1);
        BudgetDetail past(groceriesBudget(31200), month, november(), 5000, 0, QDate(2026, 12, 5));
        QCOMPARE(text(past, "perDayValueCaption"), QString("Per day"));
        QCOMPARE(text(past, "perDayValue"), Settings::instance().formatSpending(31200 / 30));
        QCOMPARE(text(past, "paceValue"), QString("(none)"));
        QCOMPARE(text(past, "leftPerDayValue"), QString("(none)"));
        QCOMPARE(text(past, "lastMonthValue"), QString("%1, no budget").arg(Settings::instance().formatSpending(5000)));

        BudgetDetail future(groceriesBudget(0), month, {}, 0, 0, QDate(2026, 10, 20));
        QCOMPARE(text(future, "perDayValue"), QString("(none)"));
        QCOMPARE(text(future, "paceValue"), QString("(none)"));
        QCOMPARE(text(future, "budgetDetailEmpty"), QString("No transactions in November yet."));
        QVERIFY(!future.findChild<QToolButton*>("budgetDetailTransactions"));
        QVERIFY(future.findChildren<HoverRow*>().isEmpty());

        BudgetDetail single(groceriesBudget(6420), month, november().mid(0, 1), 0, 0, QDate(2026, 11, 2));
        QCOMPARE(single.findChild<QToolButton*>("budgetDetailTransactions")->text(), QString("Show it on the Transactions page ›"));
    }

    void theChartFindsItsDots() {
        const QDate month(2026, 11, 1);
        BudgetMonthChart chart(month, 40000, november(), QDate(2026, 11, 18));
        chart.resize(400, 170);
        QCOMPARE(chart.dotText(5), QString("17 Nov · P6 · %1").arg(Settings::instance().formatSpending(7410)));
        for (int i = 1; i < 6; ++i) {
            QVERIFY(chart.dotCenter(i).x() > chart.dotCenter(i - 1).x());       // Later days to the right
            QVERIFY(chart.dotCenter(i).y() < chart.dotCenter(i - 1).y());       // More spent, higher
        }
        QVERIFY(chart.rect().contains(chart.dotCenter(5).toPoint()));
        QCOMPARE(chart.transactionAt(chart.dotCenter(2)), 2);
        QCOMPARE(chart.transactionAt(chart.dotCenter(2) + QPointF(3, 3)), 2);
        QCOMPARE(chart.transactionAt(QPointF(0, 0)), -1);

        QSignalSpy clicked(&chart, &BudgetMonthChart::transactionClicked);
        QTest::mouseClick(&chart, Qt::LeftButton, {}, chart.dotCenter(3).toPoint());
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(clicked.first().first().toInt(), 4);
        QTest::mouseClick(&chart, Qt::LeftButton, {}, QPoint(1, 1));
        QTest::mouseClick(&chart, Qt::RightButton, {}, chart.dotCenter(3).toPoint());
        QCOMPARE(clicked.size(), 1);

        // Hovering a dot makes it bigger and the cursor a hand
        const QImage plain = chart.grab().toImage();
        QMouseEvent onDot(QEvent::MouseMove, chart.dotCenter(3), chart.mapToGlobal(chart.dotCenter(3)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&chart, &onDot);
        QCOMPARE(chart.cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(chart.grab().toImage() != plain);
        QMouseEvent away(QEvent::MouseMove, QPointF(1, 1), chart.mapToGlobal(QPointF(1, 1)), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&chart, &away);
        QCOMPARE(chart.cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(chart.grab().toImage(), plain);

        // Leaving lets go of the dot as well
        QApplication::sendEvent(&chart, &onDot);
        QVERIFY(chart.grab().toImage() != plain);
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&chart, &leave);
        QCOMPARE(chart.grab().toImage(), plain);
    }

    void theDetailIsItsOwnCardAndItsTransactionsLightUp() {
        BudgetDetail detail(groceriesBudget(31200), QDate(2026, 11, 1), november(), 0, 0, QDate(2026, 11, 18));
        detail.resize(560, detail.sizeHint().height());
        detail.layout()->activate();
        // A card set in from the sides, rounded at every corner, one shade top to bottom
        const QImage drawn = detail.grab().toImage();
        const QRectF card = detail.cardRect();
        QCOMPARE(card.left(), BudgetDetail::Inset + 0.5);
        const auto at = [&drawn](double x, double y) { return drawn.pixelColor(QPointF(x, y).toPoint() * drawn.devicePixelRatio()); };
        const QColor outside = at(2, drawn.height() / drawn.devicePixelRatio() / 2);
        const QColor fill = at(card.center().x(), card.top() + 6);
        QVERIFY(fill != outside);
        QCOMPARE(at(card.center().x(), card.bottom() - 4), fill);       // The same shade at the bottom
        QCOMPARE(at(card.left() + 1, card.top() + 1), outside);           // Round at the top...
        QCOMPARE(at(card.left() + 1, card.bottom() - 1), outside);        // ...and at the bottom

        // Its rows sit on the hover colour already, so they light up stronger than other rows
        HoverRow* listed = detail.findChildren<HoverRow*>("budgetDetailTransaction").first();
        HoverRow other;
        other.resize(listed->size());
        HoverRow strong;
        strong.setStrongHighlight(true);
        strong.resize(listed->size());
        const QPoint middle(listed->width() / 2, listed->height() / 2);
        const QColor plain = other.grab().toImage().pixelColor(middle);
        other.setAttribute(Qt::WA_UnderMouse);
        strong.setAttribute(Qt::WA_UnderMouse);
        const QColor lit = other.grab().toImage().pixelColor(middle);
        QVERIFY(lit != plain);
        QVERIFY(strong.grab().toImage().pixelColor(middle) != lit);
        other.show();                                    // Focus lights it up as well
        other.activateWindow();                          // Wine on xvfb has no window manager to do it
        QVERIFY(QTest::qWaitForWindowActive(&other));
        other.setFocus();
        QVERIFY(other.hasFocus());
        other.setAttribute(Qt::WA_UnderMouse, false);    // Shown, it's under the test's mouse too
        QCOMPARE(other.grab().toImage().pixelColor(middle), lit);

        // Enter opens a transaction like a click; other keys don't
        QSignalSpy requested(&detail, &BudgetDetail::transactionRequested);
        QTest::keyClick(listed, Qt::Key_A);
        QCOMPARE(requested.size(), 0);
        QTest::keyClick(listed, Qt::Key_Return);
        QTest::keyClick(listed, Qt::Key_Enter);
        QCOMPARE(requested.size(), 2);
        QCOMPARE(requested.first().first().toInt(), 6);
    }

    void moneyBackStepsTheLineDown() {
        QList<BudgetTransaction> list = november().mid(0, 2);
        list.append({9, QDate(2026, 11, 8), "Shop", -3000});
        BudgetMonthChart chart(QDate(2026, 11, 1), 40000, list, QDate(2026, 11, 18));
        chart.resize(400, 170);
        QVERIFY(chart.dotCenter(2).y() > chart.dotCenter(1).y());
        // More back than spent goes below the start, and stays in the chart
        BudgetMonthChart below(QDate(2026, 11, 1), 40000, {{1, QDate(2026, 11, 3), "", -40000}}, QDate(2026, 11, 18));
        below.resize(400, 170);
        QVERIFY(below.rect().contains(below.dotCenter(0).toPoint()));
        QVERIFY(below.dotText(0).contains(" · No payee · "));
    }

    void theTilesSayHowTheMonthIsGoing() {
        const Settings& settings = Settings::instance();
        const QDate today = QDate::currentDate();
        const int days = thisMonth.daysInMonth();
        BudgetPage page;
        page.setUserId(1);
        auto* spentNote = page.findChild<QLabel*>("spentNote");
        auto* leftNote = page.findChild<QLabel*>("leftNote");
        QVERIFY(spentNote->isHidden() && leftNote->isHidden());   // Nothing to say without budgets

        // Nothing spent yet: below an even pace, all of it left for the days to come
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth.addMonths(-1), 40000));
        page.reload();
        QVERIFY(!spentNote->isHidden() && !leftNote->isHidden());
        QCOMPARE(spentNote->text(), QString("%1 below even pace").arg(settings.formatCurrency(40000 * today.day() / days)));
        QCOMPARE(leftNote->text(), QString("%1 a day until month end").arg(settings.formatCurrency(40000 / (days - today.day() + 1))));

        // Over the limit
        spend(45000, groceries);
        page.reload();
        QCOMPARE(spentNote->text(), QString("%1 above even pace").arg(settings.formatCurrency(45000 - 40000 * today.day() / days)));
        QCOMPARE(leftNote->text(), QString("Nothing left"));

        // A month that's over says how it ended; one to come says nothing
        Transaction before(-1, accountId, "Shop", 30000, thisMonth.addMonths(-1), "", groceries, "expense");
        QVERIFY(TransactionDAO().addTransaction(before) > 0);
        page.findChild<QToolButton*>("previousPeriod")->click();
        QCOMPARE(spentNote->text(), QString("Within the limit"));
        QVERIFY(leftNote->isHidden());
        QVERIFY(TransactionDAO().addTransaction(before) > 0);
        page.reload();
        QCOMPARE(spentNote->text(), QString("Over the limit"));
        page.showMonth(thisMonth.addMonths(2));
        QVERIFY(spentNote->isHidden() && leftNote->isHidden());
    }

    void theTilesGoByTheDay() {
        const QDate march(2026, 3, 1);
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, march, 31000));          // 10,00 € a day
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, accountId, "Shop", 4000, QDate(2026, 3, 2), "", groceries, "expense")) > 0);
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, accountId, "Shop", 9000, QDate(2026, 3, 25), "", groceries, "expense")) > 0);
        const QString money6000 = money(6000);
        BudgetPage page;
        page.today = [] { return QDate(2026, 3, 10); };
        page.setUserId(1);
        page.showMonth(march);
        auto* spentNote = page.findChild<QLabel*>("spentNote");
        auto* leftNote = page.findChild<QLabel*>("leftNote");

        // By the 10th: 40,00 € of an even 100,00 €; the planned 90,00 € already counts as gone, so
        // 180,00 € are left for 22 days
        QCOMPARE(spentNote->text(), QString("%1 below even pace").arg(money6000));
        QCOMPARE(leftNote->text(), QString("%1 a day until month end").arg(money(18000 / 22)));

        // The last day still goes by the pace, with everything spent by then
        page.today = [] { return QDate(2026, 3, 31); };
        page.reload();
        QCOMPARE(spentNote->text(), QString("%1 below even pace").arg(money(31000 - 13000)));
        QCOMPARE(leftNote->text(), QString("%1 a day until month end").arg(money(18000)));

        // The day after, how it ended
        page.today = [] { return QDate(2026, 4, 1); };
        page.reload();
        QCOMPARE(spentNote->text(), QString("Within the limit"));
        QVERIFY(leftNote->isHidden());
    }

    void theHeaderLeadsToTheReport() {
        BudgetPage page;
        page.setUserId(1);
        auto* link = page.findChild<QToolButton*>("budgetReportLink");
        QVERIFY(link->isHidden());                       // Nothing to report without budgets
        QVERIFY(BudgetDAO().setMonthlyLimit(1, groceries, thisMonth, 40000));
        page.reload();
        QVERIFY(!link->isHidden());
        QSignalSpy requested(&page, &BudgetPage::budgetReportRequested);
        link->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toDate(), thisMonth);
        page.findChild<QToolButton*>("previousPeriod")->click();
        QVERIFY(link->isHidden());                       // No budget the month before
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

    void rowsLightUpUnderTheMouseAndIgnoreOtherKeys() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        QAbstractButton* budget = row(page, "Groceries");
        const QImage plain = budget->grab().toImage();
        budget->setAttribute(Qt::WA_UnderMouse);
        QVERIFY(budget->grab().toImage() != plain);
        QTest::keyClick(budget, Qt::Key_A);
        QVERIFY(page.findChild<BudgetEditor*>()->isHidden());
        QVERIFY(!page.findChild<BudgetDetail*>());

        // Unfolded, no band without the mouse: the detail below is a card of its own
        budget->setAttribute(Qt::WA_UnderMouse, false);
        const QPoint edge(4, budget->height() / 2);              // Where only a band would paint
        const QImage folded = budget->grab().toImage();
        budget->click();
        QVERIFY(page.findChild<BudgetDetail*>());
        const QImage unfolded = budget->grab().toImage();
        QCOMPARE(unfolded.pixelColor(edge * unfolded.devicePixelRatio()), folded.pixelColor(edge * folded.devicePixelRatio()));
        QVERIFY(unfolded != folded);                     // Its chevron points down
    }

    void aMonthThatsOverSaysHowItEnded() {
        const MonthBudget within = groceriesBudget(31200);           // Of 400,00
        BudgetDetail past(within, QDate(2026, 10, 1), november(), 0, 0, QDate(2026, 11, 18));
        QCOMPARE(past.findChild<QLabel*>("endedValue")->text(), QString("%1 left").arg(money(8800)));
        QCOMPARE(past.findChild<QLabel*>("endedValueCaption")->text(), QString("Ended"));
        BudgetDetail over(groceriesBudget(45000), QDate(2026, 10, 1), november(), 0, 0, QDate(2026, 11, 18));
        auto* overValue = over.findChild<QLabel*>("endedValue");
        QCOMPARE(overValue->text(), QString("%1 over").arg(money(5000)));
        QCOMPARE(overValue->palette().color(QPalette::WindowText), Theme::colors().danger);
        BudgetDetail current(within, QDate(2026, 11, 1), november(), 0, 0, QDate(2026, 11, 18));
        QVERIFY(!current.findChild<QLabel*>("endedValue"));          // Still going: the pace instead
        QVERIFY(current.findChild<QLabel*>("paceValue"));
        BudgetDetail ahead(within, QDate(2026, 12, 1), {}, 0, 0, QDate(2026, 11, 18));
        QVERIFY(!ahead.findChild<QLabel*>("endedValue"));            // Not even begun
        BudgetDetail usedUp(groceriesBudget(40000), QDate(2026, 10, 1), november(), 0, 0, QDate(2026, 11, 18));
        QCOMPARE(usedUp.findChild<QLabel*>("endedValue")->text(), QString("%1 left").arg(money(0)));
    }

    void withoutAUserThereIsNothingToShow() {
        BudgetPage page;
        page.reload();
        QVERIFY(QMetaObject::invokeMethod(&page, "addBudget", Q_ARG(int, 0), Q_ARG(Cents, 0)));
        QVERIFY(page.findChildren<QAbstractButton*>("budgetRow").isEmpty());
        QVERIFY(page.findChild<BudgetEditor*>()->isHidden());
        QVERIFY(page.findChild<QToolButton*>("budgetReportLink")->isHidden());
        QVERIFY(page.findChild<QLabel*>("spentNote")->isHidden());
        QVERIFY(page.findChild<QLabel*>("leftNote")->isHidden());
    }

    void aBudgetThatCantBeRemovedStays() {
        QVERIFY(BudgetDAO().setLimitForMonth(1, groceries, thisMonth, 40000));
        BudgetPage page;
        page.setUserId(1);
        QSignalSpy changed(&page, &BudgetPage::budgetsChanged);
        edit(page, "Groceries");
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
