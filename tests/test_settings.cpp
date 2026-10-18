#include <QtTest>
#include "settings.h"

class TestSettings : public QObject {
    Q_OBJECT
    QMap<QString, QVariant> original;

private slots:
    void init() {
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) original.insert(key, settings.value(key));
    }
    void cleanup() {
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = original.cbegin(); it != original.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
        original.clear();
    }

    void currencySymbols() {
        auto& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        QCOMPARE(s.getCurrencySymbol(), QString("€"));
        QCOMPARE(s.getCurrencySymbolType(), Settings::CurrencySymbol::EUR);
        s.setCustomCurrencySymbol("¤");
        QCOMPARE(s.getCurrencySymbol(), QString("¤"));
        QCOMPARE(s.getCurrencySymbolType(), Settings::CurrencySymbol::Custom);
        QCOMPARE(s.getCustomCurrencySymbol(), QString("¤"));
    }

    void separatorsAndFormatting() {
        auto& s = Settings::instance();
        s.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Point);
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        s.setCurrencySymbolPrefix(false);
        QCOMPARE(s.formatNumber(1234567.5), QString("1.234.567,50"));
        QCOMPARE(s.formatCurrency(-1234.5), QString("-1.234,50 €"));
    }

    void settingsRoundTripAndDelimiterFallback() {
        auto& s = Settings::instance();
        s.setAutoDetectCsvDelimiter(false);
        s.setDefaultCsvDelimiter(';');
        s.setFastEntryEnabled(true);
        QCOMPARE(s.isAutoDetectCsvDelimiter(), false);
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(';'));
        QCOMPARE(s.isFastEntryEnabled(), true);
        s.setSelectionCheckboxesEnabled(true);
        QCOMPARE(s.isSelectionCheckboxesEnabled(), true);
        s.getQSettings().setValue("import/defaultCsvDelimiter", QString());
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(','));
    }

    void enumValuesAndBooleanRoundTrips() {
        auto& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::GBP);
        QCOMPARE(s.getCurrencySymbol(), QString("£"));
        s.setCurrencySymbol(Settings::CurrencySymbol::JPY);
        QCOMPARE(s.getCurrencySymbol(), QString("¥"));
        s.setCurrencySymbol(Settings::CurrencySymbol::CHF);
        QCOMPARE(s.getCurrencySymbol(), QString("CHF"));

        s.setDecimalSeparator(Settings::DecimalSeparator::Point);
        QCOMPARE(s.getDecimalSeparator(), Settings::DecimalSeparator::Point);
        s.setThousandsSeparator(Settings::ThousandsSeparator::None);
        QCOMPARE(s.getThousandsSeparator(), Settings::ThousandsSeparator::None);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Comma);
        QCOMPARE(s.getThousandsSeparator(), Settings::ThousandsSeparator::Comma);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Space);
        QCOMPARE(s.getThousandsSeparator(), Settings::ThousandsSeparator::Space);

        s.setAutoDetectDecimalSeparator(false);
        s.setCurrencySymbolPrefix(false);
        QCOMPARE(s.isAutoDetectDecimalSeparator(), false);
        QCOMPARE(s.isCurrencySymbolPrefix(), false);
    }

    void malformedValuesUseFallbacks() {
        auto& s = Settings::instance();
        QSettings& raw = s.getQSettings();
        raw.setValue("currency/symbol", "unknown");
        raw.setValue("currency/decimalSeparator", "unknown");
        raw.setValue("currency/thousandsSeparator", "unknown");
        raw.setValue("import/defaultCsvDelimiter", QString());
        QCOMPARE(s.getCurrencySymbol(), QString("$"));
        QCOMPARE(s.getDecimalSeparator(), Settings::DecimalSeparator::Point);
        QCOMPARE(s.getThousandsSeparator(), Settings::ThousandsSeparator::Comma);
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(','));
    }

    void formattingBoundariesAndOtherSettings() {
        auto& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::USD);
        s.setDecimalSeparator(Settings::DecimalSeparator::Point);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Comma);
        s.setCurrencySymbolPrefix(true);
        QCOMPARE(s.formatNumber(12.345, 2), QString("12.35"));
        QCOMPARE(s.formatNumber(0.0, 0), QString("0"));
        QCOMPARE(s.formatNumber(1234.0, 0), QString("1,234"));
        QCOMPARE(s.formatCurrency(0.0), QString("$0.00"));
        QCOMPARE(s.formatCurrency(12.5, true), QString("+$12.50"));
        QCOMPARE(s.formatCurrency(-12.5, true), QString("-$12.50"));

        s.setCurrencySymbolPrefix(false);
        QCOMPARE(s.formatCurrency(12.5), QString("12.50 $"));
        s.setTheme("Light");
        s.setDefaultUserId(-1);
        QCOMPARE(s.getTheme(), QString("Light"));
        QCOMPARE(s.getDefaultUserId(), -1);
        s.setDefaultUserId(42);
        QCOMPARE(s.getDefaultUserId(), 42);
    }

    void missingKeysUseDocumentedDefaults() {
        auto& s = Settings::instance();
        QSettings& raw = s.getQSettings();
        const QStringList keys = {
            "currency/symbol", "currency/decimalSeparator", "currency/thousandsSeparator",
            "currency/symbolPrefix", "import/autoDetectCsvDelimiter",
            "import/autoDetectDecimalSeparator", "import/defaultCsvDelimiter",
            "transactions/fastEntryEnabled", "transactions/selectionCheckboxes",
            "app/theme", "app/defaultUserId"
        };
        for (const QString& key : keys) raw.remove(key);
        QCOMPARE(s.getCurrencySymbol(), QString("$"));
        QCOMPARE(s.getDecimalSeparator(), Settings::DecimalSeparator::Point);
        QCOMPARE(s.getThousandsSeparator(), Settings::ThousandsSeparator::Comma);
        QCOMPARE(s.isCurrencySymbolPrefix(), true);
        QCOMPARE(s.isAutoDetectCsvDelimiter(), true);
        QCOMPARE(s.isAutoDetectDecimalSeparator(), true);
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(','));
        QCOMPARE(s.isFastEntryEnabled(), false);
        QCOMPARE(s.isSelectionCheckboxesEnabled(), false);
        QCOMPARE(s.getTheme(), QString("Dark"));
        QCOMPARE(s.getDefaultUserId(), -1);
    }

    void restoreDefaultsKeepsWindowState() {
        auto& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        s.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        s.setFastEntryEnabled(true);
        s.setDefaultCsvDelimiter(';');
        s.setTheme("Light");
        s.setSidebarCollapsed(true);

        s.restoreDefaults();

        QCOMPARE(s.getCurrencySymbolType(), Settings::CurrencySymbol::USD);
        QCOMPARE(s.getDecimalSeparator(), Settings::DecimalSeparator::Point);
        QCOMPARE(s.isFastEntryEnabled(), false);
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(','));
        QCOMPARE(s.getTheme(), QString("Dark"));
        QCOMPARE(s.isSidebarCollapsed(), true);
    }

    void sidebarStateDefaultsAndRoundTrip() {
        auto& s = Settings::instance();
        QSettings& raw = s.getQSettings();
        raw.remove("ui");
        QCOMPARE(s.isSidebarCollapsed(), false);
        QCOMPARE(s.isSidebarSectionExpanded("accounts"), true);

        s.setSidebarCollapsed(true);
        s.setSidebarSectionExpanded("accounts", false);
        QCOMPARE(s.isSidebarCollapsed(), true);
        QCOMPARE(s.isSidebarSectionExpanded("accounts"), false);
        QCOMPARE(s.isSidebarSectionExpanded("budget"), true);
    }
};

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
