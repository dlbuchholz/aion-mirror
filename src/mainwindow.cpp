#include "accountbalancelistwidget.h"
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
}

MainWindow::~MainWindow()
{
    delete ui;
}
