#ifndef PREFERENCESWIDGET_H
#define PREFERENCESWIDGET_H


#include <QWidget>
#include "account.h"
#include "category.h"

class QLabel;
class QTableWidget;

namespace Ui {
class PreferencesWidget;
}

// Settings section for categories and account types. Edits are staged until the
// settings page applies them (saveChanges) or reverts them (discardChanges).
class PreferencesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreferencesWidget(QWidget *parent = nullptr);
    ~PreferencesWidget();

    bool hasPendingChanges() const;
    QStringList lastErrors() const;  // What the last saveChanges() couldn't do

public slots:
    bool saveChanges();          // Save staged changes; false if some could not be saved
    void discardChanges();       // Discard all staged changes

signals:
    void pendingChangesChanged(bool hasChanges);

private slots:
    void addAccountType();
    void deleteAccountType();    // Deletes the ticked account types

    void addCategory();
    void deleteCategory();       // Deletes the ticked categories

private:
    Ui::PreferencesWidget *ui;
    AccountDAO accountDao;
    CategoryDAO categoryDao;

    void setupTables();
    void loadAccountTypes();
    void loadCategories();
    void addAccountTypeRow(int id, const QString& name);
    void addCategoryRow(int id, const QString& name, const QString& type);

    void stageAccountTypeChange(int row);
    void stageCategoryChange(int row);

    // IDs live in the (hidden) vertical header; new rows have negative temporary IDs
    static int idForRow(QTableWidget *table, int row);
    static int rowForCellWidget(QTableWidget *table, int column, const QWidget *widget);
    static QList<int> checkedRows(QTableWidget *table, int checkboxColumn);
    static bool isNameTaken(QTableWidget *table, const QString& name, int exceptRow);
    static QString uniqueName(QTableWidget *table, const QString& baseName, int exceptRow);
    void showNotice(QLabel *label, const QString& message);

    void updateButtonStates();   // Notify the settings page whether changes are staged

    int nextTempId = -1;
    QLabel *accountTypesNotice = nullptr;
    QLabel *categoriesNotice = nullptr;
    QStringList errors;

    QMap<int, QString> stagedAccounts;                     // ID -> name (new rows have negative IDs)
    QMap<int, QPair<QString, QString>> stagedCategories;   // ID -> <name, type>
    QMap<int, QString> stagedAccountDeletions;             // ID -> name (for error messages)
    QMap<int, QString> stagedCategoryDeletions;            // ID -> name
};

#endif // PREFERENCESWIDGET_H
