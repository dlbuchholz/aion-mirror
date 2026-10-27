#include <QtTest>
#include <QAbstractButton>
#include <QLabel>
#include <QMenu>

#include "account.h"
#include "budget.h"
#include "category.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/sidebarwidget.h"

class TestSidebarWidget : public QObject {
    Q_OBJECT

    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;
    int giroId = -1;
    int creditId = -1;

    // Sidebar entries are buttons; their right-hand value is the accessible description
    static QAbstractButton* item(SidebarWidget& sidebar, const QString& text) {
        for (QAbstractButton* button : sidebar.findChildren<QAbstractButton*>()) {
            if (button->text() == text) return button;
        }
        return nullptr;
    }
    static QString valueOf(SidebarWidget& sidebar, const QString& text) {
        QAbstractButton* button = item(sidebar, text);
        return button ? button->accessibleDescription() : QString("<missing %1>").arg(text);
    }
    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    static QString budgetHeaderTitle() {
        return QString("Budget · %1").arg(QDate::currentDate().toString("MMMM"));
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
        db.seedDefaultUser(1, "alex", "Alex Example");
        Settings& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        s.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Point);
        s.setCurrencySymbolPrefix(false);
        s.setSidebarBudgetSort(Settings::BudgetSort::Percentage);
        s.getQSettings().remove("ui");                   // Expanded sidebar, open sections

        giroId = AccountDAO::createAccount(1, "Girokonto", "Checking", 210000);
        creditId = AccountDAO::createAccount(1, "Kreditkarte", "Checking", -24590);
        QVERIFY(giroId > 0 && creditId > 0);
    }
    void cleanup() { db.close(); }

    void showsAccountsWithBalances() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(valueOf(sidebar, "All accounts"), QString("1.854,10 €"));
        QCOMPARE(valueOf(sidebar, "Girokonto"), QString("2.100,00 €"));
        QCOMPARE(valueOf(sidebar, "Kreditkarte"), QString("-245,90 €"));

        // New accounts appear after a refresh
        QVERIFY(AccountDAO::createAccount(1, "Tagesgeld", "Savings", 1000) > 0);
        sidebar.refresh();
        QCOMPARE(valueOf(sidebar, "Tagesgeld"), QString("10,00 €"));

        // The currency settings apply here too
        Settings::instance().setCurrencySymbol(Settings::CurrencySymbol::USD);
        Settings::instance().setCurrencySymbolPrefix(true);
        sidebar.refresh();
        QCOMPARE(valueOf(sidebar, "Kreditkarte"), QString("-$245,90"));
    }

    void clicksRequestAccountsPagesAndActions() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QSignalSpy accounts(&sidebar, &SidebarWidget::accountRequested);
        QSignalSpy addAccount(&sidebar, &SidebarWidget::addAccountRequested);
        QSignalSpy pages(&sidebar, &SidebarWidget::pageRequested);

        item(sidebar, "Kreditkarte")->click();
        item(sidebar, "All accounts")->click();
        QCOMPARE(accounts.count(), 2);
        QCOMPARE(accounts[0].first().toInt(), creditId);
        QCOMPARE(accounts[1].first().toInt(), int(SidebarWidget::ALL_ACCOUNTS_ID));

        item(sidebar, "Add account")->click();
        QCOMPARE(addAccount.count(), 1);

        item(sidebar, "Reports")->click();
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first().first().value<SidebarWidget::Page>(), SidebarWidget::Page::Reports);

        sidebar.setCurrentPage(SidebarWidget::Page::Settings);
        QVERIFY(item(sidebar, "Settings")->isChecked());
        QVERIFY(!item(sidebar, "Reports")->isChecked());
    }

    void showsTheUser() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("userItem")->text(), QString("Alex Example"));
    }

    void budgetsShowThisMonthsSpending() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        BudgetDAO budgets;
        QVERIFY(budgets.setMonthlyLimit(1, categoryId("Groceries"), monthStart, 40000));
        QVERIFY(budgets.setLimitForMonth(1, categoryId("Rent"), monthStart.addMonths(-1), 90000));  // Last month
        Transaction shopping(-1, giroId, "Rewe", 31200, today, "", categoryId("Groceries"), "expense");
        QVERIFY(TransactionDAO().addTransaction(shopping) > 0);

        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->text(), budgetHeaderTitle());
        QCOMPARE(valueOf(sidebar, "Groceries"), QString("312 € / 400 €"));
        QVERIFY(!item(sidebar, "Rent"));                 // Only budgets covering today
    }

    void showsTheMostUsedBudgetsUnlessAllAreWanted() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        CategoryDAO categories(1);
        for (const char* name : {"Transport", "Travel", "Gifts"}) {
            QVERIFY(categories.addCategory(Category(-1, name, "expense")) > 0);
        }
        // Each 100 € a month; spent: Rent 95, Travel 120 (over), Gifts 10, Groceries 60, Utilities 30,
        // Entertainment 0, Transport 80
        const QList<QPair<const char*, Cents>> spending = {{"Rent", 9500}, {"Travel", 12000}, {"Gifts", 1000},
                                                          {"Groceries", 6000}, {"Utilities", 3000},
                                                          {"Entertainment", 0}, {"Transport", 8000}};
        for (const auto& [name, cents] : spending) {
            QVERIFY(BudgetDAO().setMonthlyLimit(1, categoryId(name), monthStart, 10000));
            if (cents > 0) {
                QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giroId, name, cents, today, "", categoryId(name), "expense")) > 0);
            }
        }
        const auto shown = [](SidebarWidget& sidebar) {
            QStringList names;
            for (QAbstractButton* budget : sidebar.findChildren<QAbstractButton*>("budgetItem")) names << budget->text();
            return names;
        };

        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(shown(sidebar), (QStringList{"Travel", "Rent", "Transport", "Groceries", "Utilities"}));
        auto* more = sidebar.findChild<QAbstractButton*>("moreBudgetsItem");
        QVERIFY(more);
        QCOMPARE(more->text(), QString("+ 2 more on the Budget page"));
        QSignalSpy pages(&sidebar, &SidebarWidget::pageRequested);
        more->click();
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first().first().value<SidebarWidget::Page>(), SidebarWidget::Page::Budget);
        // The month's total counts every budget, also while the section is open: 395 of 700 €
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->accessibleDescription(), QString("56%"));

        Settings::instance().setSidebarShowingAllBudgets(true);
        sidebar.refresh();
        QCOMPARE(shown(sidebar).size(), 7);
        QVERIFY(!sidebar.findChild<QAbstractButton*>("moreBudgetsItem"));
        Settings::instance().setSidebarShowingAllBudgets(false);
    }

    void budgetsFollowTheChosenSort() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        // Limit and spent: Rent 900/450 (50%), Groceries 400/380 (95%), Utilities 200/50 (25%),
        // Entertainment 50/60 (120%)
        const QList<std::tuple<const char*, Cents, Cents>> budgets = {{"Rent", 90000, 45000},
                                                                      {"Groceries", 40000, 38000},
                                                                      {"Utilities", 20000, 5000},
                                                                      {"Entertainment", 5000, 6000}};
        for (const auto& [name, limit, spent] : budgets) {
            QVERIFY(BudgetDAO().setMonthlyLimit(1, categoryId(name), monthStart, limit));
            QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giroId, name, spent, today, "", categoryId(name), "expense")) > 0);
        }
        const auto shown = [](SidebarWidget& sidebar) {
            QStringList names;
            for (QAbstractButton* budget : sidebar.findChildren<QAbstractButton*>("budgetItem")) names << budget->text();
            return names;
        };
        Settings& settings = Settings::instance();
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(shown(sidebar), (QStringList{"Entertainment", "Groceries", "Rent", "Utilities"}));   // Percentage

        settings.setSidebarBudgetSort(Settings::BudgetSort::Spent);
        sidebar.refresh();
        QCOMPARE(shown(sidebar), (QStringList{"Rent", "Groceries", "Entertainment", "Utilities"}));
        settings.setSidebarBudgetSort(Settings::BudgetSort::Budgeted);
        sidebar.refresh();
        QCOMPARE(shown(sidebar), (QStringList{"Rent", "Groceries", "Utilities", "Entertainment"}));

        // The Budget header's right-click menu changes it too, and the sort holds when all are shown
        settings.setSidebarShowingAllBudgets(true);
        QSignalSpy changed(&sidebar, &SidebarWidget::budgetSortChanged);
        QTimer::singleShot(0, &sidebar, [&sidebar]() {
            QMenu* menu = sidebar.findChild<QMenu*>();
            QVERIFY(menu);
            QList<QAction*> actions = menu->actions();
            QCOMPARE(actions.size(), 4);
            QVERIFY(actions[2]->isChecked());                // Budgeted amount, the current one
            actions[3]->trigger();                           // Sort by name
            menu->close();
        });
        auto* header = sidebar.findChild<QAbstractButton*>("budgetHeader");
        emit header->customContextMenuRequested(QPoint(4, 4));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.getSidebarBudgetSort(), Settings::BudgetSort::Name);
        QCOMPARE(shown(sidebar), (QStringList{"Entertainment", "Groceries", "Rent", "Utilities"}));

        settings.setSidebarShowingAllBudgets(false);
        settings.setSidebarBudgetSort(Settings::BudgetSort::Percentage);
    }

    void showsWhenThereAreNoBudgets() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QVERIFY(item(sidebar, "No budgets this month"));
    }

    void foldedSectionsShowASummaryAndAreRemembered() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        QVERIFY(BudgetDAO().setLimitForMonth(1, categoryId("Rent"), monthStart, 100000));
        Transaction rent(-1, giroId, "Landlord", 25000, today, "", categoryId("Rent"), "expense");
        QVERIFY(TransactionDAO().addTransaction(rent) > 0);

        {
            SidebarWidget sidebar;
            sidebar.setUserId(1);
            auto* accountsHeader = sidebar.findChild<QAbstractButton*>("accountsHeader");
            auto* budgetHeader = sidebar.findChild<QAbstractButton*>("budgetHeader");
            QCOMPARE(accountsHeader->accessibleDescription(), QString());   // Open: no summary
            accountsHeader->click();
            budgetHeader->click();
            QCOMPARE(accountsHeader->accessibleDescription(), QString("1.604,10 €"));
            QCOMPARE(budgetHeader->accessibleDescription(), QString("25%"));
            QCOMPARE(Settings::instance().isSidebarSectionExpanded("accounts"), false);
        }

        // A new sidebar (next start) opens folded
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->accessibleDescription(), QString("25%"));
    }

    void showsTheLogoAndNameAndKeepsTheLogoWhenCollapsed() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        auto* logo = sidebar.findChild<QLabel*>("sidebarLogo");
        auto* name = sidebar.findChild<QLabel*>("sidebarBrandName");
        QVERIFY(!logo->pixmap().isNull());
        QCOMPARE(name->text(), QString("Aion"));

        sidebar.setCollapsed(true, false);
        QVERIFY(name->isHidden());
        QVERIFY(!logo->isHidden());
        sidebar.setCollapsed(false, false);
        QVERIFY(!name->isHidden());
    }

    void collapsingIsRemembered() {
        {
            SidebarWidget sidebar;
            sidebar.setUserId(1);
            QVERIFY(!sidebar.isCollapsed());
            auto* collapse = sidebar.findChild<QAbstractButton*>("collapseItem");
            collapse->click();
            QVERIFY(sidebar.isCollapsed());
            QCOMPARE(collapse->text(), QString("Expand"));
            QCOMPARE(Settings::instance().isSidebarCollapsed(), true);
        }
        SidebarWidget sidebar;
        QVERIFY(sidebar.isCollapsed());
        sidebar.setCollapsed(false, false);
        QCOMPARE(Settings::instance().isSidebarCollapsed(), false);
    }
};

QTEST_MAIN(TestSidebarWidget)
#include "test_sidebarwidget.moc"
