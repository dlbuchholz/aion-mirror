#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>

#include "account.h"
#include "category.h"
#include "tests/test_db_fixture.h"
#include "widgets/preferenceswidget.h"

class TestPreferencesWidget : public QObject {
    Q_OBJECT

    TestDbFixture db;

    static QTableWidget* categories(PreferencesWidget& widget) { return widget.findChild<QTableWidget*>("categoriesTable"); }
    static QTableWidget* accountTypes(PreferencesWidget& widget) { return widget.findChild<QTableWidget*>("accountTypesTable"); }

    static int rowNamed(QTableWidget* table, const QString& name) {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, 0)->text() == name) return row;
        }
        return -1;
    }
    static void tick(QTableWidget* table, int row, int column) {
        table->cellWidget(row, column)->findChild<QCheckBox*>()->setChecked(true);
    }
    static QStringList categoryNames() {
        QStringList names;
        for (const Category& category : CategoryDAO().getAllCategories()) names << category.getName();
        return names;
    }
    static QString categoryType(const QString& name) {
        for (const Category& category : CategoryDAO().getAllCategories()) {
            if (category.getName() == name) return category.getType();
        }
        return QString();
    }
    static void click(PreferencesWidget& widget, const char* name) {
        widget.findChild<QPushButton*>(name)->click();
    }

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void addingSeveralEntriesSavesAllOfThem() {
        // All new rows used to share ID -1, so only the last one was saved
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        click(widget, "addCategoryButton");
        click(widget, "addCategoryButton");
        table->item(table->rowCount() - 2, 0)->setText("Travel");
        table->item(table->rowCount() - 1, 0)->setText("Gifts");
        QVERIFY(widget.hasPendingChanges());

        QVERIFY(widget.saveChanges());
        QVERIFY(categoryNames().contains("Travel"));
        QVERIFY(categoryNames().contains("Gifts"));
        QCOMPARE(categoryType("Gifts"), QString("expense"));
        QVERIFY(!widget.hasPendingChanges());

        click(widget, "addAccountTypeButton");
        click(widget, "addAccountTypeButton");
        QTableWidget* types = accountTypes(widget);
        types->item(types->rowCount() - 2, 0)->setText("Credit card");
        types->item(types->rowCount() - 1, 0)->setText("Cash");
        QVERIFY(widget.saveChanges());
        const QList<QString> savedTypes = AccountDAO().getAccountTypes();
        QVERIFY(savedTypes.contains("Credit card"));
        QVERIFY(savedTypes.contains("Cash"));
    }

    void duplicateNamesAreRenamedWithAHint() {
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        click(widget, "addCategoryButton");
        click(widget, "addCategoryButton");
        table->item(table->rowCount() - 2, 0)->setText("groceries");   // Existing, different case
        table->item(table->rowCount() - 1, 0)->setText("Groceries 2"); // Clashes with the renamed one

        QCOMPARE(table->item(table->rowCount() - 2, 0)->text(), QString("groceries 2"));
        QCOMPARE(table->item(table->rowCount() - 1, 0)->text(), QString("Groceries 2 2"));
        auto* notice = widget.findChild<QLabel*>("categoriesNotice");
        QVERIFY(!notice->isHidden());
        QVERIFY(notice->text().contains("already exists"));
    }

    void tickingAloneDoesNotDelete() {
        // Ticking used to stage a deletion that any later Apply carried out
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        tick(table, rowNamed(table, "Bonus"), 2);
        table->item(rowNamed(table, "Rent"), 0)->setText("Housing");

        QVERIFY(widget.saveChanges());
        QVERIFY(categoryNames().contains("Bonus"));
        QVERIFY(categoryNames().contains("Housing"));
    }

    void deleteSelectedRemovesTickedEntries() {
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        click(widget, "addCategoryButton");
        table->item(table->rowCount() - 1, 0)->setText("Temporary");
        tick(table, table->rowCount() - 1, 2);                  // New row: just dropped
        tick(table, rowNamed(table, "Bonus"), 2);
        click(widget, "deleteCategoryButton");
        QCOMPARE(rowNamed(table, "Bonus"), -1);

        QVERIFY(widget.saveChanges());
        QVERIFY(!categoryNames().contains("Bonus"));
        QVERIFY(!categoryNames().contains("Temporary"));
    }

    void editsAfterDeletingRowsHitTheRightRow() {
        // Row numbers used to be captured when rows were created and went stale after deletions
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        const QString lastName = table->item(table->rowCount() - 1, 0)->text();
        const QString lastType = categoryType(lastName);
        tick(table, 0, 2);
        tick(table, 1, 2);
        click(widget, "deleteCategoryButton");

        auto* typeCombo = qobject_cast<QComboBox*>(table->cellWidget(table->rowCount() - 1, 1));
        typeCombo->setCurrentText(lastType == "income" ? "expense" : "income");
        QVERIFY(widget.saveChanges());
        QCOMPARE(categoryType(lastName), lastType == "income" ? QString("expense") : QString("income"));
    }

    void refusedChangesAreReported() {
        // Used to fail silently
        const int accountId = AccountDAO::createAccount(1, "Girokonto", "Savings", 0.0);
        QVERIFY(accountId > 0);
        Transaction transaction(-1, accountId, "Rewe", 5.0, QDate::currentDate(), "", 0, "expense");
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        transaction.setCategoryId(query.value(0).toInt());
        QVERIFY(TransactionDAO().addTransaction(transaction) > 0);

        PreferencesWidget widget;
        tick(categories(widget), rowNamed(categories(widget), "Groceries"), 2);
        click(widget, "deleteCategoryButton");
        tick(accountTypes(widget), rowNamed(accountTypes(widget), "Savings"), 1);
        click(widget, "deleteAccountTypeButton");

        QVERIFY(!widget.saveChanges());
        QCOMPARE(widget.lastErrors().size(), 2);
        QVERIFY(widget.lastErrors().join(' ').contains("\"Groceries\""));
        QVERIFY(widget.lastErrors().join(' ').contains("\"Savings\""));
        // Nothing was lost, and the tables show the database again
        QVERIFY(categoryNames().contains("Groceries"));
        QVERIFY(rowNamed(categories(widget), "Groceries") >= 0);
    }

    void emptyNamesAreNotSaved() {
        PreferencesWidget widget;
        QTableWidget* table = categories(widget);
        const int before = categoryNames().size();
        click(widget, "addCategoryButton");                     // Left blank

        const int rentRow = rowNamed(table, "Rent");
        table->item(rentRow, 0)->setText("   ");                // Existing name cleared: restored
        QCOMPARE(table->item(rentRow, 0)->text(), QString("Rent"));

        QVERIFY(widget.saveChanges());
        QCOMPARE(categoryNames().size(), before);
        QVERIFY(!categoryNames().contains(QString()));
    }

    void discardRestoresTheDatabaseState() {
        PreferencesWidget widget;
        QSignalSpy pending(&widget, &PreferencesWidget::pendingChangesChanged);
        QTableWidget* table = categories(widget);
        table->item(rowNamed(table, "Rent"), 0)->setText("Housing");
        QVERIFY(widget.hasPendingChanges());
        QCOMPARE(pending.last().first().toBool(), true);

        widget.discardChanges();
        QVERIFY(!widget.hasPendingChanges());
        QCOMPARE(pending.last().first().toBool(), false);
        QVERIFY(rowNamed(table, "Rent") >= 0);
        QVERIFY(categoryNames().contains("Rent"));
    }
};

QTEST_MAIN(TestPreferencesWidget)
#include "test_preferenceswidget.moc"
