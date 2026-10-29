#include "settings.h"
#include "db/databasemanager.h"
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>

Settings:: Settings()
    : settings("LeineHive", "Aion") {
    // settings(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/settings.ini",
    //           QSettings::IniFormat)
    qDebug() << "Settings file location:" << settings.fileName();

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

// The currency and number format belong to the open database (database_settings), so a
// database looks the same on every computer. A database that has none yet uses what this
// computer had before, then the defaults. Read on every formatted amount, hence the cache.
void Settings::loadDatabaseValues() const {
    DatabaseManager& manager = DatabaseManager::instance();
    if (loadedGeneration == manager.generation()) {
        return;
    }
    loadedGeneration = manager.generation();
    databaseValues.clear();
    if (!manager.database().isOpen()) {
        return;
    }
    QSqlQuery query(manager.database());
    if (query.exec("SELECT key, value FROM database_settings")) {
        while (query.next()) {
            databaseValues.insert(query.value(0).toString(), query.value(1).toString());
        }
    }
}

QVariant Settings::formatValue(const QString& key, const QVariant& defaultValue) const {
    loadDatabaseValues();
    const auto stored = databaseValues.constFind(key);
    return stored != databaseValues.constEnd() ? QVariant(*stored) : settings.value(key, defaultValue);
}

void Settings::setFormatValue(const QString& key, const QVariant& value) {
    QSqlDatabase& database = DatabaseManager::instance().database();
    if (!database.isOpen()) {
        settings.setValue(key, value);               // Only before a database is open
        return;
    }
    loadDatabaseValues();
    const auto stored = databaseValues.constFind(key);
    if (stored != databaseValues.constEnd() && *stored == value.toString()) {
        return;                                      // Unchanged: spares a write to disk (Apply saves every value)
    }
    QSqlQuery query(database);
    query.prepare("INSERT OR REPLACE INTO database_settings (key, value) VALUES (:key, :value)");
    query.bindValue(":key", key);
    query.bindValue(":value", value.toString());
    if (!query.exec()) {
        qWarning() << "Could not save" << key << "in the database:" << query.lastError().text();
        return;
    }
    databaseValues.insert(key, value.toString());
}

// Currency Symbol
QString Settings::getCurrencySymbol() const {
    QString symbolType = formatValue("currency/symbol", "USD").toString();

    if (symbolType == "Custom") {
        return formatValue("currency/customSymbol", "$").toString();
    }

    if (symbolType == "USD") return "$";
    if (symbolType == "EUR") return "€";
    if (symbolType == "GBP") return "£";
    if (symbolType == "JPY") return "¥";
    if (symbolType == "CHF") return "CHF";

    return "$";  // Default
}

Settings::CurrencySymbol Settings::getCurrencySymbolType() const {
    const QString symbolType = formatValue("currency/symbol", "USD").toString();
    if (symbolType == "EUR") return CurrencySymbol::EUR;
    if (symbolType == "GBP") return CurrencySymbol::GBP;
    if (symbolType == "JPY") return CurrencySymbol::JPY;
    if (symbolType == "CHF") return CurrencySymbol::CHF;
    if (symbolType == "Custom") return CurrencySymbol::Custom;
    return CurrencySymbol::USD;  // Default
}

QString Settings::getCustomCurrencySymbol() const {
    return formatValue("currency/customSymbol", "$").toString();
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
    setFormatValue("currency/symbol", symbolStr);
}

void Settings:: setCustomCurrencySymbol(const QString& symbol) {
    setFormatValue("currency/customSymbol", symbol);
    setCurrencySymbol(CurrencySymbol::Custom);
}

// Decimal Separator
Settings::DecimalSeparator Settings:: getDecimalSeparator() const {
    QString sep = formatValue("currency/decimalSeparator", "Point").toString();
    return (sep == "Comma") ? DecimalSeparator::Comma : DecimalSeparator::Point;
}

void Settings::setDecimalSeparator(DecimalSeparator separator) {
    QString sepStr = (separator == DecimalSeparator:: Comma) ? "Comma" : "Point";
    setFormatValue("currency/decimalSeparator", sepStr);
}

char Settings::getDecimalSeparatorChar() const {
    return (getDecimalSeparator() == DecimalSeparator::Comma) ? ',' : '.';
}

// Thousands Separator
Settings::ThousandsSeparator Settings::getThousandsSeparator() const {
    QString sep = formatValue("currency/thousandsSeparator", "Comma").toString();

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
    setFormatValue("currency/thousandsSeparator", sepStr);
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
    return formatValue("currency/symbolPrefix", true).toBool();
}

void Settings::setCurrencySymbolPrefix(bool isPrefix) {
    setFormatValue("currency/symbolPrefix", isPrefix);
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

bool Settings::isCategorySuggestionEnabled() const {
    return settings.value("transactions/suggestCategories", true).toBool();
}

void Settings::setCategorySuggestionEnabled(bool enabled) {
    settings.setValue("transactions/suggestCategories", enabled);
}

// Format Currency
QString Settings::formatCurrency(Cents amount, bool showSign, int decimals) const {
    QString formatted = formatNumber(qAbs(amount), decimals);
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
QString Settings::formatSpending(Cents cents, int decimals) const {
    const QString amount = formatCurrency(cents, false, decimals);
    return cents < 0 ? QString("%1 (refunded)").arg(amount) : amount;
}

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
int Settings::getDefaultUserId(const QString& databasePath) const {
    return settings.value("app/defaultUsers").toMap().value(databasePath, -1).toInt();
}

void Settings::setDefaultUserId(const QString& databasePath, int userId) {
    QVariantMap users = settings.value("app/defaultUsers").toMap();
    if (userId > 0) {
        users.insert(databasePath, userId);
    } else {
        users.remove(databasePath);
    }
    settings.setValue("app/defaultUsers", users);
    settings.remove("app/defaultUserId");            // One user for every database, before 0.1.0
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
    if (DatabaseManager::instance().database().isOpen()) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.exec("DELETE FROM database_settings WHERE key LIKE 'currency/%'");
        loadedGeneration = quint64(-1);              // Reload
    }
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

bool Settings::isSidebarShowingAllBudgets() const {
    return settings.value("appearance/sidebarAllBudgets", false).toBool();
}

void Settings::setSidebarShowingAllBudgets(bool showAll) {
    settings.setValue("appearance/sidebarAllBudgets", showAll);
}

// Stored by name, so the value stays readable in the advanced settings
static const QStringList BUDGET_SORT_NAMES = {"percentage", "spent", "budgeted", "name"};

Settings::BudgetSort Settings::getSidebarBudgetSort() const {
    const int index = BUDGET_SORT_NAMES.indexOf(settings.value("appearance/sidebarBudgetSort").toString());
    return index < 0 ? BudgetSort::Percentage : static_cast<BudgetSort>(index);
}

void Settings::setSidebarBudgetSort(BudgetSort sort) {
    settings.setValue("appearance/sidebarBudgetSort", BUDGET_SORT_NAMES.value(static_cast<int>(sort)));
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

// Reports page
QString Settings::getReportsRange() const {
    return settings.value("ui/reportsRange", "6months").toString();
}

void Settings::setReportsRange(const QString& key) {
    settings.setValue("ui/reportsRange", key);
}

QDate Settings::getReportsFrom() const {
    return QDate::fromString(settings.value("ui/reportsFrom").toString(), Qt::ISODate);
}

QDate Settings::getReportsTo() const {
    return QDate::fromString(settings.value("ui/reportsTo").toString(), Qt::ISODate);
}

void Settings::setReportsCustomDays(const QDate& from, const QDate& to) {
    settings.setValue("ui/reportsFrom", from.toString(Qt::ISODate));
    settings.setValue("ui/reportsTo", to.toString(Qt::ISODate));
}
