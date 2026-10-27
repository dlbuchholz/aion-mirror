#ifndef SETTINGS_H
#define SETTINGS_H

#include <QHash>
#include <QSettings>
#include <QStringList>
#include <QString>
#include <QLocale>
#include "money.h"

class Settings {
public:
    // Currency settings
    enum class CurrencySymbol {
        USD,    // $
        EUR,    // €
        GBP,    // £
        JPY,    // ¥
        CHF,    // CHF
        Custom
    };

    enum class DecimalSeparator {
        Point,  // 1234.56
        Comma   // 1234,56
    };

    enum class ThousandsSeparator {
        None,   // 1234.56
        Comma,  // 1,234.56
        Point,  // 1.234,56
        Space   // 1 234,56
    };

    // Get singleton instance
    static Settings& instance();

    QSettings& getQSettings();

    // Currency settings
    QString getCurrencySymbol() const;
    CurrencySymbol getCurrencySymbolType() const;
    QString getCustomCurrencySymbol() const;
    void setCurrencySymbol(CurrencySymbol symbol);
    void setCustomCurrencySymbol(const QString& symbol);

    DecimalSeparator getDecimalSeparator() const;
    void setDecimalSeparator(DecimalSeparator separator);

    ThousandsSeparator getThousandsSeparator() const;
    void setThousandsSeparator(ThousandsSeparator separator);

    bool isCurrencySymbolPrefix() const;  // true = $100, false = 100$
    void setCurrencySymbolPrefix(bool isPrefix);

    // Import settings
    bool isAutoDetectCsvDelimiter() const;
    void setAutoDetectCsvDelimiter(bool autoDetect);

    bool isAutoDetectDecimalSeparator() const;
    void setAutoDetectDecimalSeparator(bool autoDetect);

    QChar getDefaultCsvDelimiter() const;
    void setDefaultCsvDelimiter(QChar delimiter);

    // Transaction settings
    bool isFastEntryEnabled() const;
    void setFastEntryEnabled(bool enabled);

    bool isSelectionCheckboxesEnabled() const;
    void setSelectionCheckboxesEnabled(bool enabled);

    // New transactions get a category, and are offered a payee name, from the rules while they're
    // filled in (on by default)
    bool isCategorySuggestionEnabled() const;
    void setCategorySuggestionEnabled(bool enabled);

    // Formatting methods
    QString formatCurrency(Cents amount, bool showSign = false, int decimals = 2) const;   // Sign shown for negatives (and + if showSign)
    QString formatNumber(Cents cents, int decimals = 2) const;          // Absolute value; decimals is 2 or 0

    // Other app settings (expandable)
    QString getTheme() const;
    void setTheme(const QString& theme);

    // Category colours on the Dashboard's spending bars and as dots on budget rows (tags are always coloured)
    bool isCategoryColorsInOverviewsEnabled() const;
    void setCategoryColorsInOverviewsEnabled(bool enabled);

    // Every budget in the sidebar, instead of the five most used (off by default)
    bool isSidebarShowingAllBudgets() const;
    void setSidebarShowingAllBudgets(bool showAll);

    // Order of the sidebar's budgets, which also picks the five shown: amounts largest first, names A–Z
    enum class BudgetSort {
        Percentage,     // Share of the limit spent (default)
        Spent,
        Budgeted,
        Name
    };
    BudgetSort getSidebarBudgetSort() const;
    void setSidebarBudgetSort(BudgetSort sort);

    // User opened automatically at start, -1 for none. Remembered per database file, since user
    // IDs mean different people in different databases.
    int getDefaultUserId(const QString& databasePath) const;
    void setDefaultUserId(const QString& databasePath, int userId);

    // Database files
    QString getLastDatabase() const;                 // Empty: use the default database
    void setLastDatabase(const QString& path);
    QStringList getRecentDatabases() const;          // Most recent first
    void addRecentDatabase(const QString& path);
    void removeRecentDatabase(const QString& path);

    // Removes all user-facing settings (currency, transactions, import, theme) so their defaults apply.
    // Window state such as the sidebar layout is kept.
    void restoreDefaults();

    // Sidebar state (persisted between sessions)
    bool isSidebarCollapsed() const;
    void setSidebarCollapsed(bool collapsed);

    bool isSidebarSectionExpanded(const QString& section) const;
    void setSidebarSectionExpanded(const QString& section, bool expanded);

private:
    Settings();  // Private constructor for singleton
    ~Settings() = default;

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    QSettings settings;

    // Currency and number format, stored in the open database
    QVariant formatValue(const QString& key, const QVariant& defaultValue) const;
    void setFormatValue(const QString& key, const QVariant& value);
    void loadDatabaseValues() const;
    mutable QHash<QString, QString> databaseValues;
    mutable quint64 loadedGeneration = quint64(-1);

    // Helper methods
    QString getCurrencySymbolString(CurrencySymbol symbol) const;
    char getDecimalSeparatorChar() const;
    QString getThousandsSeparatorString() const;
};

#endif // SETTINGS_H
