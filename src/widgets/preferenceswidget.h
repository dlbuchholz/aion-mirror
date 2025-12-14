#ifndef PREFERENCESWIDGET_H
#define PREFERENCESWIDGET_H


#include <QWidget>
#include "account.h"
#include "category.h"

#include "../../ui/ui_preferenceswidget.h"

class PreferencesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreferencesWidget(QWidget *parent = nullptr);
    ~PreferencesWidget();

private slots:
    void saveChanges();          // Save staged changes to the database
    void discardChanges();       // Discard all staged changes

    void addAccountType();       // Add a new Account Type
    void deleteAccountType();    // Delete an Account Type

    void addCategory();          // Add a new Category
    void deleteCategory();       // Delete a Category

private:
    Ui::PreferencesWidget *ui;
    AccountDAO accountDao;
    CategoryDAO categoryDao;

    void setupTables();          // Configure table headers and properties
    void loadAccountTypes();     // Load data into the Account Types table
    void loadCategories();       // Load data into the Categories table

    void stageAccountTypeChange(int row);  // Stage changes for Account Types
    void stageCategoryChange(int row);     // Stage changes for Categories

    void updateButtonStates();   // Dynamically show/hide Save and Discard buttons

    QMap<int, QString> stagedAccounts;                     // Staged Account Type changes (ID -> Name)
    QMap<int, QPair<QString, QString>> stagedCategories;   // Staged Category changes (ID -> <Name, Type>)
    QSet<int> stagedAccountDeletions;                      // Staged Account IDs for deletion
    QSet<int> stagedCategoryDeletions;                     // Staged Category IDs for deletion
};

#endif // PREFERENCESWIDGET_H
