#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

#include "category.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/settingspage.h"

class TestSettingsPage : public QObject {
    Q_OBJECT

    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;

    template <typename T>
    static T* child(SettingsPage& page, const char* name) {
        T* widget = page.findChild<T*>(name);
        if (!widget) qFatal("Missing widget %s", name);
        return widget;
    }

    static void select(QComboBox* combo, const QVariant& data) {
        combo->setCurrentIndex(combo->findData(data));
    }

    static bool hasUnsavedChanges(SettingsPage& page) {
        return child<QPushButton>(page, "applyButton")->isEnabled()
               && !child<QPushButton>(page, "revertButton")->isHidden()
               && !child<QLabel>(page, "unsavedLabel")->isHidden();
    }

private slots:
    void initTestCase() {
        db.init();
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }
    void cleanupTestCase() {
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }
    void init() {
        db.yankDb();
        db.seedDefaultUser();
        Settings& s = Settings::instance();
        s.restoreDefaults();
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        s.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Point);
        s.setCurrencySymbolPrefix(false);
        s.setFastEntryEnabled(true);
        s.setTheme("Light");
        s.setDefaultCsvDelimiter(';');
    }
    void cleanup() { db.close(); }

    void showsTheStoredValues() {
        SettingsPage page;
        QCOMPARE(child<QComboBox>(page, "currencyCombo")->currentData().toInt(), int(Settings::CurrencySymbol::EUR));
        QVERIFY(!child<QLineEdit>(page, "customSymbolEdit")->isEnabled());
        QCOMPARE(child<QComboBox>(page, "symbolPositionCombo")->currentData().toBool(), false);
        QCOMPARE(child<QComboBox>(page, "decimalSeparatorCombo")->currentData().toInt(), int(Settings::DecimalSeparator::Comma));
        QCOMPARE(child<QComboBox>(page, "thousandsSeparatorCombo")->currentData().toInt(), int(Settings::ThousandsSeparator::Point));
        QCOMPARE(child<QCheckBox>(page, "fastEntryCheck")->isChecked(), true);
        QCOMPARE(child<QCheckBox>(page, "selectionCheckboxesCheck")->isChecked(), false);
        QCOMPARE(child<QComboBox>(page, "themeCombo")->currentData().toString(), QString("Light"));
        QCOMPARE(child<QComboBox>(page, "defaultDelimiterCombo")->currentData().toString(), QString(";"));
        QVERIFY(!hasUnsavedChanges(page));
    }

    void changesWaitForApply() {
        SettingsPage page;
        QSignalSpy changed(&page, &SettingsPage::settingsChanged);

        select(child<QComboBox>(page, "decimalSeparatorCombo"), int(Settings::DecimalSeparator::Point));
        child<QCheckBox>(page, "selectionCheckboxesCheck")->setChecked(true);
        QVERIFY(hasUnsavedChanges(page));
        QCOMPARE(Settings::instance().getDecimalSeparator(), Settings::DecimalSeparator::Comma);   // Not yet

        child<QPushButton>(page, "applyButton")->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(Settings::instance().getDecimalSeparator(), Settings::DecimalSeparator::Point);
        QCOMPARE(Settings::instance().isSelectionCheckboxesEnabled(), true);
        QVERIFY(!hasUnsavedChanges(page));
        QVERIFY(child<QPushButton>(page, "revertButton")->isHidden());
    }

    void revertRestoresTheStoredValues() {
        SettingsPage page;
        QComboBox* theme = child<QComboBox>(page, "themeCombo");
        select(theme, "Dark");
        QVERIFY(hasUnsavedChanges(page));

        child<QPushButton>(page, "revertButton")->click();
        QCOMPARE(theme->currentData().toString(), QString("Light"));
        QVERIFY(!hasUnsavedChanges(page));
        QCOMPARE(Settings::instance().getTheme(), QString("Light"));
    }

    void customCurrencySymbol() {
        SettingsPage page;
        QComboBox* currency = child<QComboBox>(page, "currencyCombo");
        QLineEdit* symbol = child<QLineEdit>(page, "customSymbolEdit");

        // Custom without a symbol keeps the stored currency
        select(currency, int(Settings::CurrencySymbol::Custom));
        QVERIFY(symbol->isEnabled());
        symbol->clear();
        child<QPushButton>(page, "applyButton")->click();
        QCOMPARE(Settings::instance().getCurrencySymbol(), QString("€"));

        QTest::keyClicks(symbol, "Fr.");
        QVERIFY(hasUnsavedChanges(page));
        child<QPushButton>(page, "applyButton")->click();
        QCOMPARE(Settings::instance().getCurrencySymbolType(), Settings::CurrencySymbol::Custom);
        QCOMPARE(Settings::instance().getCurrencySymbol(), QString("Fr."));
        QCOMPARE(Settings::instance().formatCurrency(-1234.5), QString("-1.234,50 Fr."));
    }

    void categoryEditsUseTheSameApplyButton() {
        SettingsPage page;
        auto* categories = page.findChild<QTableWidget*>("categoriesTable");
        QVERIFY(!hasUnsavedChanges(page));
        categories->item(0, 0)->setText("Renamed category");
        QVERIFY(hasUnsavedChanges(page));

        child<QPushButton>(page, "applyButton")->click();
        QVERIFY(!hasUnsavedChanges(page));
        bool found = false;
        for (const Category& category : CategoryDAO().getAllCategories()) found |= category.getName() == "Renamed category";
        QVERIFY(found);
    }
};

QTEST_MAIN(TestSettingsPage)
#include "test_settingspage.moc"
