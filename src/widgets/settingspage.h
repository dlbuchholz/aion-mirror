#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include "transaction.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QToolButton;
class AccountTypesSettings;
class CategoriesSettings;
class RulesSettings;
struct Rule;

// Settings page of the main window: one section per settings group plus
// categories and account types, saved together with Apply.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setUserId(int userId);                      // Whose categories and account types to show
    void reloadData();                               // Categories and account types changed elsewhere (an import)
    // Shows Rules with the editor filled with draft (a new rule, or with an ID that rule changed)
    void openRuleDraft(const Rule& draft, const QList<Transaction>& examples = {});
    void reloadSidebarBudgetSort();                  // Changed from the sidebar; other unsaved edits stay

signals:
    void settingsChanged();                          // Emitted after changes were applied
    void dataChanged();                              // Categories or account types were edited (saved right away)

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void apply();
    void revert();
    void restoreDefaults();
    void openAdvancedSettings();

private:
    QFormLayout* addFormSection(const QString &title, const QString &icon);
    void addSection(const QString &title, const QString &icon, QWidget *content);
    void updateIcons();                              // Re-tint button icons after a theme change
    void buildCurrencySection();
    void buildTransactionsSection();
    void buildAppearanceSection();
    void buildImportSection();
    void buildCategoriesSection();
    void buildAboutSection();

    void watch(QComboBox *combo);
    void watch(QCheckBox *checkBox);
    void watch(QLineEdit *lineEdit);
    void markFormChanged();
    void updateChangeState();

    void loadSettings();
    void saveSettings();

    QListWidget *sectionList = nullptr;
    QStackedWidget *sectionStack = nullptr;
    QToolButton *restoreDefaultsButton = nullptr;
    QToolButton *advancedButton = nullptr;
    QLabel *unsavedLabel = nullptr;
    QPushButton *revertButton = nullptr;
    QPushButton *applyButton = nullptr;
    bool formChanged = false;
    bool loading = false;

    // Currency
    QComboBox *currencyCombo = nullptr;
    QLineEdit *customSymbolEdit = nullptr;
    QComboBox *symbolPositionCombo = nullptr;
    QComboBox *decimalSeparatorCombo = nullptr;
    QComboBox *thousandsSeparatorCombo = nullptr;

    // Transactions
    QCheckBox *fastEntryCheck = nullptr;
    QCheckBox *selectionCheckboxesCheck = nullptr;
    QCheckBox *suggestCategoriesCheck = nullptr;
    QCheckBox *categoryColorsCheck = nullptr;
    QCheckBox *allBudgetsCheck = nullptr;
    QComboBox *budgetSortCombo = nullptr;

    // Appearance
    QComboBox *themeCombo = nullptr;

    // Import
    QCheckBox *autoDetectDelimiterCheck = nullptr;
    QComboBox *defaultDelimiterCombo = nullptr;
    QCheckBox *autoDetectDecimalCheck = nullptr;

    // Categories and account types
    CategoriesSettings *categoriesSettings = nullptr;
    RulesSettings *rulesSettings = nullptr;
    AccountTypesSettings *accountTypesSettings = nullptr;
};

#endif // SETTINGSPAGE_H
