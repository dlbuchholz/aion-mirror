#include "widgets/addaccountwidget.h"
#include "widgets/budgetpage.h"
#include "widgets/dashboardpage.h"
#include "widgets/reportspage.h"
#include "widgets/settingspage.h"
#include "widgets/transactionspage.h"
#include "mainwindow.h"
#include "settings.h"
#include "theme.h"
#include "../ui/ui_mainwindow.h"

#include <QDialog>
#include <QVBoxLayout>

MainWindow::MainWindow(int userId, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), userId(userId),
      appliedTheme(Theme::fromName(Settings::instance().getTheme()))   // Applied at start, before the window
{
    ui->setupUi(this);
    ui->sidebar->setUserId(userId);
    ui->transactionsPage->setUserId(userId);
    ui->budgetPage->setUserId(userId);
    ui->dashboardPage->setUserId(userId);
    ui->reportsPage->setUserId(userId);
    ui->settingsPage->setUserId(userId);

    connect(ui->sidebar, &SidebarWidget::pageRequested, this, &MainWindow::showPage);
    connect(ui->sidebar, &SidebarWidget::accountRequested, this, &MainWindow::onAccountRequested);
    connect(ui->sidebar, &SidebarWidget::addAccountRequested, this, &MainWindow::onAddAccountClicked);
    connect(ui->sidebar, &SidebarWidget::correctBalanceRequested, ui->transactionsPage, &TransactionsPage::correctBalance);
    connect(ui->sidebar, &SidebarWidget::switchUserRequested, this, &MainWindow::switchUserRequested);
    connect(ui->sidebar, &SidebarWidget::budgetSortChanged, ui->settingsPage, &SettingsPage::reloadSidebarBudgetSort);
    connect(ui->settingsPage, &SettingsPage::settingsChanged, this, &MainWindow::onSettingsChanged);
    connect(ui->settingsPage, &SettingsPage::dataChanged, this, [this]() {
        // Category names, colours and merges show up everywhere
        ui->transactionsPage->reload();
        ui->budgetPage->reload();
        ui->dashboardPage->reload();
        ui->sidebar->refresh();
    });

    // Balances and budget usage depend on the saved transactions
    connect(ui->transactionsPage, &TransactionsPage::transactionsChanged,
            ui->sidebar, &SidebarWidget::refresh);
    connect(ui->transactionsPage, &TransactionsPage::transactionsChanged,
            ui->budgetPage, &BudgetPage::reload);
    connect(ui->budgetPage, &BudgetPage::budgetsChanged, ui->sidebar, &SidebarWidget::refresh);
    connect(ui->budgetPage, &BudgetPage::budgetsChanged, ui->dashboardPage, &DashboardPage::reload);
    connect(ui->transactionsPage, &TransactionsPage::transactionsChanged,
            ui->dashboardPage, &DashboardPage::reload);
    // Imports can add categories, and category usage counts follow the transactions
    connect(ui->transactionsPage, &TransactionsPage::transactionsChanged,
            ui->settingsPage, &SettingsPage::reloadData);

    // Dashboard shortcuts into the other pages
    DashboardPage* dashboard = ui->dashboardPage;
    TransactionsPage* transactions = ui->transactionsPage;
    connect(dashboard, &DashboardPage::addTransactionRequested, this, [this, transactions]() {
        showPage(SidebarWidget::Page::Transactions);
        transactions->addTransaction();
    });
    connect(dashboard, &DashboardPage::importRequested, transactions, &TransactionsPage::importTransactions);
    connect(dashboard, &DashboardPage::transactionsRequested, this, [this]() {
        onAccountRequested(SidebarWidget::ALL_ACCOUNTS_ID);
    });
    connect(dashboard, &DashboardPage::transactionRequested, this, [this, transactions](int transactionId) {
        showPage(SidebarWidget::Page::Transactions);
        transactions->openTransaction(transactionId);
    });
    connect(dashboard, &DashboardPage::categoryRequested, this, [this, transactions](int categoryId) {
        showPage(SidebarWidget::Page::Transactions);
        transactions->showCategoryThisMonth(categoryId);
    });
    connect(dashboard, &DashboardPage::budgetRequested, this, [this]() { showPage(SidebarWidget::Page::Budget); });

    // Reports lead on to the transactions they counted
    connect(ui->reportsPage, &ReportsPage::transactionsRequested, this,
            [this, transactions](int categoryId, int accountId, const QDate& from, const QDate& to) {
        transactions->showCategoryInRange(categoryId, accountId < 0 ? TransactionsPage::ALL_ACCOUNTS : accountId, from, to);
        showPage(SidebarWidget::Page::Transactions);
    });
    connect(ui->reportsPage, &ReportsPage::payeeTransactionsRequested, this,
            [this, transactions](const QString& payee, int accountId, const QDate& from, const QDate& to) {
        transactions->showPayeeInRange(payee, accountId < 0 ? TransactionsPage::ALL_ACCOUNTS : accountId, from, to);
        showPage(SidebarWidget::Page::Transactions);
    });

    // "Create rule from this…" and "Add to rule" on transactions open Settings > Rules with the rule filled in
    connect(transactions, &TransactionsPage::ruleDraftRequested, this,
            [this](const Rule& draft, const QList<Transaction>& examples) {
        showPage(SidebarWidget::Page::Settings);
        ui->settingsPage->openRuleDraft(draft, examples);
    });

    showPage(SidebarWidget::Page::Dashboard);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showPage(SidebarWidget::Page page) {
    if (page == SidebarWidget::Page::Dashboard) {
        ui->dashboardPage->reload();               // Greeting and "days left" follow the clock
    } else if (page == SidebarWidget::Page::Reports) {
        ui->reportsPage->reload();                 // Transactions and accounts may have changed elsewhere
    }
    ui->pageStack->setCurrentIndex(static_cast<int>(page));
    ui->sidebar->setCurrentPage(page);
}

void MainWindow::onAccountRequested(int accountId) {
    if (accountId == SidebarWidget::ALL_ACCOUNTS_ID) {
        ui->transactionsPage->showAccount(TransactionsPage::ALL_ACCOUNTS);
    } else {
        ui->transactionsPage->showAccount(accountId);
    }
    showPage(SidebarWidget::Page::Transactions);
}

void MainWindow::onAddAccountClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("Add account");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new AddAccountWidget(&dialog);
    form->setUserId(userId);
    layout->addWidget(form);
    connect(form, &AddAccountWidget::accountCreated, &dialog, &QDialog::accept);

    if (dialog.exec() == QDialog::Accepted) {
        reloadAccounts();
    }
}

void MainWindow::reloadAccounts() {
    ui->sidebar->refresh();
    ui->transactionsPage->reload();
    ui->budgetPage->reload();
    ui->dashboardPage->reload();
}

void MainWindow::onSettingsChanged() {
    qDebug() << "Settings changed, refreshing UI";
    // Restyling every widget is the slow part of Apply, so only when the theme changed
    const Theme::Type theme = Theme::fromName(Settings::instance().getTheme());
    if (theme != appliedTheme) {
        Theme::applyTheme(theme);
        appliedTheme = theme;
    }

    // Number formats and categories may have changed
    ui->transactionsPage->applySettings();
    ui->budgetPage->applySettings();
    ui->dashboardPage->applySettings();
    ui->reportsPage->applySettings();
    ui->transactionsPage->reload();
    ui->sidebar->refresh();
}
