#include "settings.h"
#include <QLocale>

Settings:: Settings()
    : settings("LeineHive", "Aion") {
    // settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/settings.ini",
    //           QSettings::IniFormat)
    qDebug() << "Settings file location:" << settings.fileName();

    // Initialize with defaults if not set
    if (! settings.contains("currency/symbol")) {
        setCurrencySymbol(CurrencySymbol::USD);
    }
    if (!settings.contains("currency/decimalSeparator")) {
        setDecimalSeparator(DecimalSeparator::Point);
    }
    if (!settings.contains("currency/thousandsSeparator")) {
        setThousandsSeparator(ThousandsSeparator::Comma);
    }
    if (!settings.contains("currency/symbolPrefix")) {
        setCurrencySymbolPrefix(true);
    }

    // Import defaults
    if (!settings.contains("import/autoDetectCsvDelimiter")) {
        setAutoDetectCsvDelimiter(true);  // Auto-detect by default
    }
    if (!settings.contains("import/autoDetectDecimalSeparator")) {
        setAutoDetectDecimalSeparator(true);  // Auto-detect by default
    }
    if (!settings.contains("import/defaultCsvDelimiter")) {
        setDefaultCsvDelimiter(',');  // Comma by default
    }
}

Settings& Settings::instance() {
    static Settings instance;
    return instance;
}

QSettings &Settings::getQSettings()
{
    return settings;
}

// Currency Symbol
QString Settings::getCurrencySymbol() const {
    QString symbolType = settings.value("currency/symbol", "USD").toString();

    if (symbolType == "Custom") {
        return settings.value("currency/customSymbol", "$").toString();
    }

    if (symbolType == "USD") return "$";
    if (symbolType == "EUR") return "€";
    if (symbolType == "GBP") return "£";
    if (symbolType == "JPY") return "¥";
    if (symbolType == "CHF") return "CHF";

    return "$";  // Default
}

Settings::CurrencySymbol Settings::getCurrencySymbolType() const {
    const QString symbolType = settings.value("currency/symbol", "USD").toString();
    if (symbolType == "EUR") return CurrencySymbol::EUR;
    if (symbolType == "GBP") return CurrencySymbol::GBP;
    if (symbolType == "JPY") return CurrencySymbol::JPY;
    if (symbolType == "CHF") return CurrencySymbol::CHF;
    if (symbolType == "Custom") return CurrencySymbol::Custom;
    return CurrencySymbol::USD;  // Default
}

QString Settings::getCustomCurrencySymbol() const {
    return settings.value("currency/customSymbol", "$").toString();
}

void Settings:: setCurrencySymbol(CurrencySymbol symbol) {
    QString symbolStr;
    switch (symbol) {
    case CurrencySymbol::USD: symbolStr = "USD"; break;
    case CurrencySymbol::EUR: symbolStr = "EUR"; break;
    case CurrencySymbol:: GBP: symbolStr = "GBP"; break;
    case CurrencySymbol::JPY: symbolStr = "JPY"; break;
    case CurrencySymbol::CHF: symbolStr = "CHF"; break;
    case CurrencySymbol::Custom:  symbolStr = "Custom"; break;
    }
    settings.setValue("currency/symbol", symbolStr);
}

void Settings:: setCustomCurrencySymbol(const QString& symbol) {
    settings.setValue("currency/customSymbol", symbol);
    setCurrencySymbol(CurrencySymbol::Custom);
}

// Decimal Separator
Settings::DecimalSeparator Settings:: getDecimalSeparator() const {
    QString sep = settings.value("currency/decimalSeparator", "Point").toString();
    return (sep == "Comma") ? DecimalSeparator::Comma : DecimalSeparator::Point;
}

void Settings::setDecimalSeparator(DecimalSeparator separator) {
    QString sepStr = (separator == DecimalSeparator:: Comma) ? "Comma" : "Point";
    settings. setValue("currency/decimalSeparator", sepStr);
}

char Settings::getDecimalSeparatorChar() const {
    return (getDecimalSeparator() == DecimalSeparator::Comma) ? ',' : '.';
}

// Thousands Separator
Settings::ThousandsSeparator Settings::getThousandsSeparator() const {
    QString sep = settings.value("currency/thousandsSeparator", "Comma").toString();

    if (sep == "None") return ThousandsSeparator:: None;
    if (sep == "Point") return ThousandsSeparator::Point;
    if (sep == "Space") return ThousandsSeparator::Space;
    return ThousandsSeparator:: Comma;  // Default
}

void Settings:: setThousandsSeparator(ThousandsSeparator separator) {
    QString sepStr;
    switch (separator) {
    case ThousandsSeparator::None: sepStr = "None"; break;
    case ThousandsSeparator:: Comma: sepStr = "Comma"; break;
    case ThousandsSeparator::Point: sepStr = "Point"; break;
    case ThousandsSeparator::Space: sepStr = "Space"; break;
    }
    settings.setValue("currency/thousandsSeparator", sepStr);
}

QString Settings::getThousandsSeparatorString() const {
    switch (getThousandsSeparator()) {
    case ThousandsSeparator::None: return "";
    case ThousandsSeparator::Comma: return ",";
    case ThousandsSeparator::Point: return ".";
    case ThousandsSeparator::Space: return " ";
    }
    return ",";
}

// Currency Symbol Position
bool Settings::isCurrencySymbolPrefix() const {
    return settings.value("currency/symbolPrefix", true).toBool();
}

void Settings::setCurrencySymbolPrefix(bool isPrefix) {
    settings.setValue("currency/symbolPrefix", isPrefix);
}

// Auto-detect CSV delimiter setting
bool Settings::isAutoDetectCsvDelimiter() const {
    return settings.value("import/autoDetectCsvDelimiter", true).toBool();
}

void Settings::setAutoDetectCsvDelimiter(bool autoDetect) {
    settings.setValue("import/autoDetectCsvDelimiter", autoDetect);
}

// Auto-detect decimal separator setting
bool Settings::isAutoDetectDecimalSeparator() const {
    return settings.value("import/autoDetectDecimalSeparator", true).toBool();
}

void Settings::setAutoDetectDecimalSeparator(bool autoDetect) {
    settings.setValue("import/autoDetectDecimalSeparator", autoDetect);
}

// Default CSV delimiter
QChar Settings::getDefaultCsvDelimiter() const {
    QString delimiterStr = settings.value("import/defaultCsvDelimiter", ",").toString();
    if (delimiterStr. isEmpty()) return ',';
    return delimiterStr[0];
}

void Settings:: setDefaultCsvDelimiter(QChar delimiter) {
    settings.setValue("import/defaultCsvDelimiter", QString(delimiter));
}

// Fast entry: keep the editor open for the next new transaction after saving one
bool Settings::isFastEntryEnabled() const {
    return settings.value("transactions/fastEntryEnabled", false).toBool();
}

void Settings::setFastEntryEnabled(bool enabled) {
    settings.setValue("transactions/fastEntryEnabled", enabled);
}

// Selection checkboxes in the transaction list (Ctrl/Shift-click works either way)
bool Settings::isSelectionCheckboxesEnabled() const {
    return settings.value("transactions/selectionCheckboxes", false).toBool();
}

void Settings::setSelectionCheckboxesEnabled(bool enabled) {
    settings.setValue("transactions/selectionCheckboxes", enabled);
}

// Format Currency
QString Settings::formatCurrency(Cents amount, bool showSign) const {
    QString formatted = formatNumber(qAbs(amount), 2);
    QString symbol = getCurrencySymbol();

    // Add currency symbol
    if (isCurrencySymbolPrefix()) {
        formatted = symbol + formatted;
    } else {
        formatted = formatted + " " + symbol;
    }

    // Add sign if requested
    if (showSign || amount < 0) {
        if (amount < 0) {
            formatted = "-" + formatted;
        } else if (showSign && amount > 0) {
            formatted = "+" + formatted;
        }
    }

    return formatted;
}

// Format Number
QString Settings::formatNumber(Cents cents, int decimals) const {
    // Integer arithmetic only: whole units, plus two decimals unless decimals == 0 (rounded half up)
    const Cents absolute = qAbs(cents);
    QString integerPart = QString::number(decimals > 0 ? absolute / 100 : (absolute + 50) / 100);
    QString decimalPart = decimals > 0 ? QString("%1").arg(absolute % 100, 2, 10, QChar('0')) : QString();

    // Add thousands separator
    QString thousandsSep = getThousandsSeparatorString();
    if (! thousandsSep.isEmpty() && integerPart.length() > 3) {
        QString formatted;
        int count = 0;
        for (int i = integerPart.length() - 1; i >= 0; --i) {
            if (count == 3) {
                formatted. prepend(thousandsSep);
                count = 0;
            }
            formatted.prepend(integerPart[i]);
            count++;
        }
        integerPart = formatted;
    }

    // Combine with decimal separator
    if (decimals > 0 && ! decimalPart.isEmpty()) {
        char decimalSep = getDecimalSeparatorChar();
        return integerPart + decimalSep + decimalPart;
    }

    return integerPart;
}

// Theme
QString Settings::getTheme() const {
    return settings.value("app/theme", "Dark").toString();
}

void Settings::setTheme(const QString& theme) {
    settings.setValue("app/theme", theme);
}

// Default User ID
int Settings::getDefaultUserId() const {
    return settings.value("app/defaultUserId", -1).toInt();
}

void Settings::setDefaultUserId(int userId) {
    settings.setValue("app/defaultUserId", userId);
}

// Database files
QString Settings::getLastDatabase() const {
    return settings.value("app/lastDatabase").toString();
}

void Settings::setLastDatabase(const QString& path) {
    settings.setValue("app/lastDatabase", path);
}

QStringList Settings::getRecentDatabases() const {
    return settings.value("app/recentDatabases").toStringList();
}

void Settings::addRecentDatabase(const QString& path) {
    constexpr int MAX_RECENT = 8;
    QStringList recent = getRecentDatabases();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > MAX_RECENT) {
        recent.removeLast();
    }
    settings.setValue("app/recentDatabases", recent);
}

void Settings::removeRecentDatabase(const QString& path) {
    QStringList recent = getRecentDatabases();
    recent.removeAll(path);
    settings.setValue("app/recentDatabases", recent);
}

void Settings::restoreDefaults() {
    settings.remove("currency");
    settings.remove("transactions");
    settings.remove("import");
    settings.remove("app/theme");
    settings.remove("appearance");
    settings.sync();
}

bool Settings::isCategoryColorsInOverviewsEnabled() const {
    return settings.value("appearance/categoryColorsInOverviews", true).toBool();
}

void Settings::setCategoryColorsInOverviewsEnabled(bool enabled) {
    settings.setValue("appearance/categoryColorsInOverviews", enabled);
}

// Sidebar state
bool Settings::isSidebarCollapsed() const {
    return settings.value("ui/sidebarCollapsed", false).toBool();
}

void Settings::setSidebarCollapsed(bool collapsed) {
    settings.setValue("ui/sidebarCollapsed", collapsed);
}

bool Settings::isSidebarSectionExpanded(const QString& section) const {
    return settings.value(QString("ui/sidebarSection/%1").arg(section), true).toBool();
}

void Settings::setSidebarSectionExpanded(const QString& section, bool expanded) {
    settings.setValue(QString("ui/sidebarSection/%1").arg(section), expanded);
}
