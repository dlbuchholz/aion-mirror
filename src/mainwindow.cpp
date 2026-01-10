#include "dialog/settingsdialog.h"
#include "widgets/accountbalancelistwidget.h"
#include "mainwindow.h"
#include "theme.h"
#include "../ui/ui_mainwindow.h"

MainWindow::MainWindow(int userId, QWidget *parent)
    : QMainWindow(parent), userId(userId), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Theme::applyTheme(Theme::Type::Dark);
    ui->accountBalanceListWidget->setUserId(userId);
    ui->transactionListWidget->setUserId(userId);

    // Connect transactions changed signal to update account balances
    connect(ui->transactionListWidget, &TransactionListWidget::transactionsChanged,
            ui->accountBalanceListWidget, &AccountBalanceListWidget::updateWidget);

    connect(ui->accountBalanceListWidget, &AccountBalanceListWidget::accountsChanged, [this, userId]() {
        ui->transactionListWidget->setUserId(userId);  // will re-populate entire widget, when accounts have changed
    });

    connect(ui->settingsButton, &QAbstractButton::clicked, this, &MainWindow::onSettingsClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onSettingsClicked() {
    SettingsDialog dialog(this);

    // Connect to refresh UI when settings change
    connect(&dialog, &SettingsDialog::settingsChanged, [this]() {
        qDebug() << "Settings changed, refreshing UI";
        // Refresh widgets
        ui->accountBalanceListWidget->updateWidget();
        ui->transactionListWidget->reinitialize();
    });

    dialog.exec();
}
