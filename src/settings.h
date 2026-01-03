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

    // Formatting methods
    QString formatCurrency(double amount, bool showSign = false) const;
    QString formatNumber(double number, int decimals = 2) const;

    // Other app settings (expandable)
    QString getTheme() const;
    void setTheme(const QString& theme);

    int getDefaultUserId() const;
    void setDefaultUserId(int userId);

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
