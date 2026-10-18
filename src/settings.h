#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>
#include <QString>
#include <QLocale>

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
    bool isLazyLoadingEnabled() const;
    void setLazyLoadingEnabled(bool enabled);

    bool isDetailEditEnabled() const;
    void setDetailEditEnabled(bool enabled);

    // Formatting methods
    QString formatCurrency(double amount, bool showSign = false) const;
    QString formatNumber(double number, int decimals = 2) const;

    // Other app settings (expandable)
    QString getTheme() const;
    void setTheme(const QString& theme);

    int getDefaultUserId() const;
    void setDefaultUserId(int userId);

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

    // Helper methods
    QString getCurrencySymbolString(CurrencySymbol symbol) const;
    char getDecimalSeparatorChar() const;
    QString getThousandsSeparatorString() const;
};

#endif // SETTINGS_H
