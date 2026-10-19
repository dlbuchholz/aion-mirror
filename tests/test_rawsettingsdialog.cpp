#include <QtTest>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

#include "dialog/rawsettingsdialog.h"
#include "settings.h"

class TestRawSettingsDialog : public QObject {
    Q_OBJECT
    QMap<QString, QVariant> originalSettings;

    static QSettings& settings() { return Settings::instance().getQSettings(); }

    static int rowOf(QTableWidget* table, const QString& key) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, 0)->text() == key) return row;
        }
        return -1;
    }

private slots:
    void initTestCase() {
        for (const QString& key : settings().allKeys()) originalSettings.insert(key, settings().value(key));
    }
    void init() {
        settings().clear();
        settings().setValue("zeta/last", "z");
        settings().setValue("Alpha/first", "a");
        settings().setValue("app/theme", "Dark");
    }
    void cleanupTestCase() {
        settings().clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings().setValue(it.key(), it.value());
    }

    void listsEverySettingSortedByName() {
        RawSettingsDialog dialog;
        auto* table = dialog.findChild<QTableWidget*>();
        QCOMPARE(table->rowCount(), 3);
        QCOMPARE(table->item(0, 0)->text(), QString("Alpha/first"));        // Case doesn't matter for the order
        QCOMPARE(table->item(1, 0)->text(), QString("app/theme"));
        QCOMPARE(table->item(1, 1)->text(), QString("Dark"));
        QVERIFY(!(table->item(1, 0)->flags() & Qt::ItemIsEditable));         // Names can't be edited
    }

    void searchFindsNamesAndValues() {
        RawSettingsDialog dialog;
        auto* table = dialog.findChild<QTableWidget*>();
        auto* search = dialog.findChild<QLineEdit*>();
        search->setText("THEME");
        QVERIFY(!table->isRowHidden(rowOf(table, "app/theme")));
        QVERIFY(table->isRowHidden(rowOf(table, "zeta/last")));
        search->setText("z");                                                // Matches the value "z" too
        QVERIFY(!table->isRowHidden(rowOf(table, "zeta/last")));
        search->clear();
        QVERIFY(!table->isRowHidden(rowOf(table, "Alpha/first")));
    }

    void applySavesEditsAndCancelDoesNot() {
        RawSettingsDialog dialog;
        QSignalSpy changed(&dialog, &RawSettingsDialog::settingsChanged);
        auto* table = dialog.findChild<QTableWidget*>();
        auto* buttons = dialog.findChild<QDialogButtonBox*>();
        auto* apply = buttons->button(QDialogButtonBox::Apply);
        QVERIFY(!apply->isEnabled());

        table->item(rowOf(table, "app/theme"), 1)->setText("Light");
        QVERIFY(apply->isEnabled());
        QCOMPARE(settings().value("app/theme").toString(), QString("Dark"));  // Not saved yet
        apply->click();
        QCOMPARE(settings().value("app/theme").toString(), QString("Light"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!apply->isEnabled());

        table->item(rowOf(table, "zeta/last"), 1)->setText("changed");
        buttons->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(settings().value("zeta/last").toString(), QString("z"));
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
    }

    void okSavesAndCloses() {
        RawSettingsDialog dialog;
        auto* table = dialog.findChild<QTableWidget*>();
        table->item(rowOf(table, "Alpha/first"), 1)->setText("b");
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
        QCOMPARE(settings().value("Alpha/first").toString(), QString("b"));
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }
};

QTEST_MAIN(TestRawSettingsDialog)
#include "test_rawsettingsdialog.moc"
