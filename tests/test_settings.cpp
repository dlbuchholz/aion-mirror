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
        s.setCustomCurrencySymbol("¤");
        QCOMPARE(s.getCurrencySymbol(), QString("¤"));
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
        s.setLazyLoadingEnabled(false);
        QCOMPARE(s.isAutoDetectCsvDelimiter(), false);
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(';'));
        QCOMPARE(s.isLazyLoadingEnabled(), false);
        s.getQSettings().setValue("import/defaultCsvDelimiter", QString());
        QCOMPARE(s.getDefaultCsvDelimiter(), QChar(','));
    }
};

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
