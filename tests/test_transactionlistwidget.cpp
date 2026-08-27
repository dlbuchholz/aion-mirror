#include <QtTest>
#include <QComboBox>
#include <QTableWidget>

#include "ui_transactionlistwidget.h"
#include "account.h"
#include "checkablecombobox.h"
#include "tests/test_db_fixture.h"
#include "widgets/transactionlistwidget.h"

class TestTransactionListWidget : public QObject {
    Q_OBJECT

private:
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void checkedIdsFollowDisplayOrder() {
        CheckableComboBox combo;
        combo.addItemWithId("First", 10);
        combo.addItemWithId("Second", 20);
        combo.setItemChecked(1, true);
        combo.setItemChecked(0, true);
        QCOMPARE(combo.checkedItemIdsInOrder(), QList<int>({10, 20}));
    }

    void newTransactionUsesFirstCheckedAccount() {
        AccountDAO accounts;
        const int firstId = accounts.createAccount(1, "First", "Checking", 0.0);
        const int secondId = accounts.createAccount(1, "Second", "Checking", 0.0);
        QVERIFY(firstId > 0);
        QVERIFY(secondId > 0);

        TransactionListWidget widget;
        widget.setUserId(1);
        auto* filter = widget.findChild<CheckableComboBox*>("checkableComboBox");
        auto* table = widget.findChild<QTableWidget*>("transactionTableWidget");
        QVERIFY(filter);
        QVERIFY(table);

        filter->uncheckAllItems();
        for (int index = 0; index < filter->itemCount(); ++index) {
            filter->setItemChecked(index, filter->checkedItemIdsInOrder().isEmpty()
                                              && index == 1);
        }
        QCOMPARE(filter->checkedItemIdsInOrder(), QList<int>({secondId}));

        QVERIFY(QMetaObject::invokeMethod(&widget, "addEmptyTransactionRow"));
        auto* accountCombo = qobject_cast<QComboBox*>(table->cellWidget(table->rowCount() - 1, 1));
        QVERIFY(accountCombo);
        QCOMPARE(accountCombo->currentData().toInt(), secondId);
    }
};

QTEST_MAIN(TestTransactionListWidget)
#include "test_transactionlistwidget.moc"
