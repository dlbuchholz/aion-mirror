#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include "dashboard.h"

class CashFlowChart;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// Start page: greeting, this month's money at a glance, cash flow of the last months,
// budget progress, where the money went and the latest transactions.
class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);

    void setUserId(int userId);
    void reload();                                   // Re-read everything (also refreshes the greeting)
    void applySettings();                            // Number formats changed

signals:
    void addTransactionRequested();
    void importRequested();
    void transactionsRequested();                    // "All transactions"
    void transactionRequested(int transactionId);    // Open one in the editor
    void categoryRequested(int categoryId);          // This month's transactions of a category (0 = none)
    void budgetRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    struct Tile {
        QLabel* value = nullptr;
        QLabel* detail = nullptr;
    };

    Tile createTile(const QString& name, const QString& caption, QLayout* layout);
    QFrame* createCard(const QString& title, QVBoxLayout** body, QToolButton** link = nullptr, const QString& linkText = {});
    void setDetail(QLabel* label, const QString& text, const QColor& color = QColor());
    void updateTiles();
    void updateBudgets();
    void updateCategories();
    void updateRecent();
    void updateIcons();

    int userId = -1;
    QString userName;
    DashboardData data;

    QLabel* greetingLabel = nullptr;
    QLabel* dateLabel = nullptr;
    QPushButton* importButton = nullptr;
    QPushButton* addButton = nullptr;
    Tile balanceTile;
    Tile incomeTile;
    Tile expensesTile;
    Tile savedTile;
    Tile averageTile;
    CashFlowChart* chart = nullptr;
    QLabel* cashFlowTitle = nullptr;
    QLabel* budgetTitle = nullptr;
    QVBoxLayout* budgetLayout = nullptr;
    QVBoxLayout* categoryLayout = nullptr;
    QLabel* categoryTitle = nullptr;
    QVBoxLayout* recentLayout = nullptr;
    QList<QToolButton*> links;
};

#endif // DASHBOARDPAGE_H
