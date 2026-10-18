#include <QtTest>
#include <QAbstractButton>

#include "account.h"
#include "budget.h"
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
        s.getQSettings().remove("ui");                   // Expanded sidebar, open sections

        giroId = AccountDAO::createAccount(1, "Girokonto", "Checking", 2100.0);
        creditId = AccountDAO::createAccount(1, "Kreditkarte", "Checking", -245.9);
        QVERIFY(giroId > 0 && creditId > 0);
    }
    void cleanup() { db.close(); }

    void showsAccountsWithBalances() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(valueOf(sidebar, "All accounts"), QString("1.854,10 €"));
        QCOMPARE(valueOf(sidebar, "Girokonto"), QString("2.100,00"));
        QCOMPARE(valueOf(sidebar, "Kreditkarte"), QString("-245,90"));   // Sign used to be dropped

        // New accounts appear after a refresh
        QVERIFY(AccountDAO::createAccount(1, "Tagesgeld", "Savings", 10.0) > 0);
        sidebar.refresh();
        QCOMPARE(valueOf(sidebar, "Tagesgeld"), QString("10,00"));
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
        QVERIFY(budgets.createBudget(Budget(1, 1, "Groceries", 400.0, monthStart, monthStart.addMonths(1).addDays(-1))));
        QVERIFY(budgets.createBudget(Budget(2, 1, "Rent", 900.0, monthStart.addMonths(-1), monthStart.addDays(-1))));  // Last month
        Transaction shopping(-1, giroId, "Rewe", 312.0, today, "", 0, "expense");
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        shopping.setCategoryId(query.value(0).toInt());
        QVERIFY(TransactionDAO().addTransaction(shopping) > 0);

        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->text(), budgetHeaderTitle());
        QCOMPARE(valueOf(sidebar, "Groceries"), QString("312 / 400"));
        QVERIFY(!item(sidebar, "Rent"));                 // Only budgets covering today
    }

    void showsWhenThereAreNoBudgets() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QVERIFY(item(sidebar, "No budgets this month"));
    }

    void foldedSectionsShowASummaryAndAreRemembered() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        QVERIFY(BudgetDAO().createBudget(Budget(1, 1, "Rent", 1000.0, monthStart, monthStart.addMonths(1).addDays(-1))));
        Transaction rent(-1, giroId, "Landlord", 250.0, today, "", 0, "expense");
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Rent'") && query.next());
        rent.setCategoryId(query.value(0).toInt());
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
