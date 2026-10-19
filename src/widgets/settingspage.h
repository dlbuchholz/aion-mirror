#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>

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

// Settings page of the main window: one section per settings group plus
// categories and account types, saved together with Apply.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void reloadData();                               // Categories and account types changed elsewhere (an import)

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
    QCheckBox *categoryColorsCheck = nullptr;

    // Appearance
    QComboBox *themeCombo = nullptr;

    // Import
    QCheckBox *autoDetectDelimiterCheck = nullptr;
    QComboBox *defaultDelimiterCombo = nullptr;
    QCheckBox *autoDetectDecimalCheck = nullptr;

    // Categories and account types
    CategoriesSettings *categoriesSettings = nullptr;
    AccountTypesSettings *accountTypesSettings = nullptr;
};

#endif // SETTINGSPAGE_H
