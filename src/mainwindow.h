#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "theme.h"
#include "widgets/sidebarwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(int userId, QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void switchUserRequested();

private slots:
    void onSettingsChanged();
    void onAddAccountClicked();
    void onAccountRequested(int accountId);

private:
    void showPage(SidebarWidget::Page page);
    void reloadAccounts();                           // Refresh everything that lists accounts

    Ui::MainWindow *ui;
    int userId;
    Theme::Type appliedTheme;                        // The theme setting last applied
};
#endif // MAINWINDOW_H
