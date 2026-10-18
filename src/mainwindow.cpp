#include "widgets/addaccountwidget.h"
#include "widgets/settingspage.h"
#include "mainwindow.h"
#include "settings.h"
#include "theme.h"
#include "../ui/ui_mainwindow.h"

#include <QDialog>
#include <QVBoxLayout>

MainWindow::MainWindow(int userId, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), userId(userId)
{
    ui->setupUi(this);
    ui->sidebar->setUserId(userId);
    ui->transactionListWidget->setUserId(userId);

    connect(ui->sidebar, &SidebarWidget::pageRequested, this, &MainWindow::showPage);
    connect(ui->sidebar, &SidebarWidget::accountRequested, this, &MainWindow::onAccountRequested);
    connect(ui->sidebar, &SidebarWidget::addAccountRequested, this, &MainWindow::onAddAccountClicked);
    connect(ui->sidebar, &SidebarWidget::switchUserRequested, this, &MainWindow::switchUserRequested);
    connect(ui->settingsPage, &SettingsPage::settingsChanged, this, &MainWindow::onSettingsChanged);

    // Balances and budget usage depend on the saved transactions
    connect(ui->transactionListWidget, &TransactionListWidget::transactionsChanged,
            ui->sidebar, &SidebarWidget::refresh);

    showPage(SidebarWidget::Page::Transactions);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showPage(SidebarWidget::Page page) {
    ui->pageStack->setCurrentIndex(static_cast<int>(page));
    ui->sidebar->setCurrentPage(page);
}

void MainWindow::onAccountRequested(int accountId) {
    if (accountId == SidebarWidget::ALL_ACCOUNTS_ID) {
        ui->transactionListWidget->showAccounts({});
    } else {
        ui->transactionListWidget->showAccounts({accountId});
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
    ui->transactionListWidget->setUserId(userId);  // will re-populate entire widget, when accounts have changed
}

void MainWindow::onSettingsChanged() {
    qDebug() << "Settings changed, refreshing UI";
    Theme::applyTheme(Theme::fromName(Settings::instance().getTheme()));
    ui->transactionListWidget->reinitialize();

    // Number formats and categories may have changed
    ui->sidebar->refresh();
}
