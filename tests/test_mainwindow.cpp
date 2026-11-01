#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTimer>
#include <functional>

#include "account.h"
#include "category.h"
#include "mainwindow.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "theme.h"
#include "transactionlistmodel.h"
#include "widgets/budgetpage.h"
#include "widgets/dashboardpage.h"
#include "widgets/reportspage.h"
#include "widgets/settingspage.h"
#include "widgets/sidebarwidget.h"
#include "widgets/transactioneditor.h"
#include "widgets/transactionspage.h"

class TestMainWindow : public QObject {
    Q_OBJECT
    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;
    int giro = -1;
    int savings = -1;
    int groceries = 0;

    static QWidget* currentPage(MainWindow& window) {
        return window.findChild<QStackedWidget*>("pageStack")->currentWidget();
    }

    static int shownTransactions(MainWindow& window) {
        auto* list = window.findChild<TransactionsPage*>()->findChild<QListView*>("transactionList");
        return static_cast<TransactionListModel*>(list->model())->totals().count;
    }

    int spend(int account, Cents cents, int category = 0) {
        return TransactionDAO().addTransaction(Transaction(-1, account, "Shop", cents, QDate::currentDate(), "", category, "expense"));
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
    }
    void init() {
        db.yankDb();
        QVERIFY(UserDAO().createUser(User(1, "alex", "Alex Example")));
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        savings = AccountDAO::createAccount(1, "Tagesgeld", "Savings", 0);
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        groceries = query.value(0).toInt();
    }
    void cleanup() { db.close(); }

    void applyingOnlyRedoesWhatChanged() {
        // Apply used to write every number format to disk and restyle every widget, whatever changed
        const auto databaseChanges = []() {
            QSqlQuery query(DatabaseManager::instance().database());
            return query.exec("SELECT total_changes()") && query.next() ? query.value(0).toLongLong() : -1;
        };
        const QString originalTheme = Settings::instance().getTheme();
        MainWindow window(1);
        auto* settings = window.findChild<SettingsPage*>();
        auto* apply = settings->findChild<QPushButton*>("applyButton");
        auto* check = settings->findChild<QCheckBox*>("selectionCheckboxesCheck");
        check->setChecked(!check->isChecked());
        apply->click();                                  // A new database stores its number formats once
        const QStyle* style = qApp->style();
        const qint64 changesBefore = databaseChanges();

        check->setChecked(!check->isChecked());
        apply->click();
        QCOMPARE(databaseChanges(), changesBefore);      // Formats unchanged: nothing written
        QCOMPARE(qApp->style(), style);                  // Theme unchanged: not restyled
        QCOMPARE(Settings::instance().isSelectionCheckboxesEnabled(), check->isChecked());

        auto* decimal = settings->findChild<QComboBox*>("decimalSeparatorCombo");
        decimal->setCurrentIndex(decimal->currentIndex() == 0 ? 1 : 0);
        apply->click();
        QCOMPARE(databaseChanges(), changesBefore + 1);  // Just the changed format

        auto* theme = settings->findChild<QComboBox*>("themeCombo");
        theme->setCurrentIndex(theme->findData(Theme::isDark() ? "Light" : "Dark"));
        const bool wasDark = Theme::isDark();
        apply->click();
        QCOMPARE(Theme::isDark(), !wasDark);             // A new theme still applies

        Settings::instance().setTheme(originalTheme);
        Theme::applyTheme(Theme::fromName(originalTheme));
    }

    void startsOnTheDashboard() {
        MainWindow window(1);
        QCOMPARE(currentPage(window), window.findChild<DashboardPage*>());
        QVERIFY(window.findChild<QLabel*>("greetingLabel")->text().endsWith(", Alex"));
    }

    void sidebarSwitchesPagesAndAccounts() {
        spend(giro, 1000);
        spend(savings, 2000);
        spend(savings, 3000);
        MainWindow window(1);
        auto* sidebar = window.findChild<SidebarWidget*>();

        emit sidebar->pageRequested(SidebarWidget::Page::Budget);
        QCOMPARE(currentPage(window), window.findChild<BudgetPage*>());
        emit sidebar->pageRequested(SidebarWidget::Page::Settings);
        QCOMPARE(currentPage(window), window.findChild<SettingsPage*>());

        emit sidebar->accountRequested(savings);
        QCOMPARE(currentPage(window), window.findChild<TransactionsPage*>());
        QCOMPARE(shownTransactions(window), 2);
        emit sidebar->accountRequested(SidebarWidget::ALL_ACCOUNTS_ID);
        QCOMPARE(shownTransactions(window), 3);

        QSignalSpy switchUser(&window, &MainWindow::switchUserRequested);
        emit sidebar->switchUserRequested();
        QCOMPARE(switchUser.count(), 1);
    }

    void dashboardShortcutsOpenTheRightPlace() {
        const int transactionId = spend(giro, 1250, groceries);
        MainWindow window(1);
        auto* dashboard = window.findChild<DashboardPage*>();
        auto* transactions = window.findChild<TransactionsPage*>();
        auto* editor = transactions->findChild<TransactionEditor*>();

        emit dashboard->addTransactionRequested();
        QCOMPARE(currentPage(window), transactions);
        QVERIFY(!editor->isHidden());
        QVERIFY(editor->isEditingNew());

        emit dashboard->transactionRequested(transactionId);
        QCOMPARE(editor->editedTransactionId(), transactionId);

        emit dashboard->categoryRequested(groceries);
        QCOMPARE(transactions->findChild<QComboBox*>("categoryFilter")->currentData().toInt(), groceries);

        emit dashboard->budgetRequested();
        QCOMPARE(currentPage(window), window.findChild<BudgetPage*>());
        emit dashboard->transactionsRequested();
        QCOMPARE(currentPage(window), transactions);
        QCOMPARE(shownTransactions(window), 1);
    }

    void reportsLeadToTheTransactionsTheyCounted() {
        const QDate today = QDate::currentDate();
        spend(giro, 1000, groceries);
        spend(savings, 2000, groceries);
        TransactionDAO().addTransaction(Transaction(-1, giro, "Bakery", 500, today, "", 0, "expense"));
        MainWindow window(1);
        auto* reports = window.findChild<ReportsPage*>();
        auto* transactions = window.findChild<TransactionsPage*>();

        emit reports->transactionsRequested(groceries, ReportsPage::ALL_ACCOUNTS, today, today);
        QCOMPARE(currentPage(window), transactions);
        QCOMPARE(shownTransactions(window), 2);
        emit reports->transactionsRequested(groceries, savings, today, today);
        QCOMPARE(shownTransactions(window), 1);

        emit window.findChild<SidebarWidget*>()->pageRequested(SidebarWidget::Page::Reports);
        emit reports->payeeTransactionsRequested("Bakery", ReportsPage::ALL_ACCOUNTS, today, today);
        QCOMPARE(currentPage(window), transactions);
        QCOMPARE(transactions->findChild<QLineEdit*>("searchEdit")->text(), QString("Bakery"));
        QCOMPARE(shownTransactions(window), 1);
    }

    void ruleDraftsOpenInTheRulesSettings() {
        MainWindow window(1);
        Rule draft;
        draft.name = "From a transaction";
        draft.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::Contains, "REWE")};
        emit window.findChild<TransactionsPage*>()->ruleDraftRequested(draft, {});
        QCOMPARE(currentPage(window), window.findChild<SettingsPage*>());
        QCOMPARE(window.findChild<SettingsPage*>()->findChild<QListWidget*>()->currentItem()->text(), QString("Rules"));
        QCOMPARE(window.findChild<QLineEdit*>("ruleNameEdit")->text(), QString("From a transaction"));
    }

    // The account dialogs block until answered, so the answer is queued first

    static void whenADialogOpens(const std::function<void(QWidget*)>& answer) {
        QTimer::singleShot(0, qApp, [answer]() { answer(QApplication::activeModalWidget()); });
    }

    static QComboBox* reportAccounts(MainWindow& window) {
        return window.findChild<ReportsPage*>()->findChild<QComboBox*>("reportsAccountCombo");
    }

    void accountsChangedFromTheSidebarShowEverywhere() {
        MainWindow window(1);
        auto* sidebar = window.findChild<SidebarWidget*>();
        auto* accounts = reportAccounts(window);

        // Cancelled: nothing changes
        whenADialogOpens([](QWidget* dialog) { qobject_cast<QDialog*>(dialog)->reject(); });
        emit sidebar->addAccountRequested();
        QCOMPARE(accounts->findText("Depot"), -1);

        whenADialogOpens([](QWidget* dialog) {
            dialog->findChild<QLineEdit*>("accountNameInput")->setText("Depot");
            dialog->findChild<QAbstractButton*>("createAccountButton")->click();
        });
        emit sidebar->addAccountRequested();
        QVERIFY(accounts->findText("Depot") >= 0);

        whenADialogOpens([](QWidget* dialog) {
            dialog->findChild<QLineEdit*>("accountNameEdit")->setText("Hausbank");
            dialog->findChild<QPushButton*>("saveAccountButton")->click();
        });
        emit sidebar->editAccountRequested(giro);
        QVERIFY(accounts->findText("Hausbank") >= 0);
        QCOMPARE(accounts->findText("Girokonto"), -1);

        whenADialogOpens([](QWidget* dialog) { qobject_cast<QDialog*>(dialog)->reject(); });
        emit sidebar->deleteAccountRequested(savings);
        QVERIFY(accounts->findText("Tagesgeld") >= 0);
        QVERIFY(AccountDAO().getAccountById(savings).getAccountId() > 0);

        whenADialogOpens([](QWidget* dialog) { dialog->findChild<QPushButton*>("deleteAccountButton")->click(); });
        emit sidebar->deleteAccountRequested(savings);
        QCOMPARE(accounts->findText("Tagesgeld"), -1);
    }

    void changedTransactionsReachTheOtherPages() {
        MainWindow window(1);
        auto* expenses = window.findChild<QLabel*>("expensesValue");
        QCOMPARE(expenses->text(), Settings::instance().formatCurrency(0));

        spend(giro, 4200, groceries);
        emit window.findChild<TransactionsPage*>()->transactionsChanged();
        QCOMPARE(expenses->text(), Settings::instance().formatCurrency(4200));
    }

    void editedCategoriesReachTheOtherPages() {
        MainWindow window(1);
        auto* filter = window.findChild<TransactionsPage*>()->findChild<QComboBox*>("categoryFilter");
        QCOMPARE(filter->findText("Travel"), -1);

        QVERIFY(CategoryDAO(1).addCategory(Category(-1, "Travel", "expense")) > 0);
        emit window.findChild<SettingsPage*>()->dataChanged();
        QVERIFY(filter->findText("Travel") >= 0);
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
