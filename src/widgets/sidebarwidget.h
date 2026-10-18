#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include <QVariantAnimation>

class QVBoxLayout;
class SidebarItem;

// Left-hand navigation: page links, collapsible account and budget sections,
// and settings / user / collapse controls pinned to the bottom.
class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    enum class Page {
        Dashboard,
        Transactions,
        Budget,
        Reports,
        Settings
    };
    Q_ENUM(Page)

    static constexpr int ALL_ACCOUNTS_ID = -1;

    explicit SidebarWidget(QWidget* parent = nullptr);

    void setUserId(int userId);
    void refresh();                                  // Reload user, accounts and budgets
    void setCurrentPage(Page page);

    bool isCollapsed() const;
    void setCollapsed(bool collapsed, bool animate = true);

signals:
    void pageRequested(SidebarWidget::Page page);
    void accountRequested(int accountId);            // ALL_ACCOUNTS_ID for all accounts
    void addAccountRequested();
    void switchUserRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Section {
        SidebarItem* header = nullptr;
        QWidget* body = nullptr;
        QVBoxLayout* bodyLayout = nullptr;
        QString key;
        QString summary;                             // Shown in the header while the section is folded
        bool expanded = true;
    };

    SidebarItem* addNavItem(QVBoxLayout* layout, const QString& icon, const QString& text, Page page);
    Section createSection(QVBoxLayout* layout, const QString& key, const QString& title);
    void setSectionExpanded(Section& section, bool expanded);
    void clearSection(Section& section);

    void loadUser();
    void loadAccounts();
    void loadBudgets();
    void showUserMenu();

    int userId = -1;
    bool collapsed = false;

    QButtonGroup navGroup;
    QWidget* sectionsArea = nullptr;
    Section accountsSection;
    Section budgetSection;
    SidebarItem* userItem = nullptr;
    SidebarItem* collapseItem = nullptr;
    QList<SidebarItem*> labelledItems;               // Items whose label hides in the collapsed rail
    QVariantAnimation widthAnimation;
};

#endif // SIDEBARWIDGET_H
