#include <QtTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>

#include "account.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "transactionlistmodel.h"
#include "widgets/transactioneditor.h"
#include "widgets/transactionspage.h"

class TestTransactionsPage : public QObject {
    Q_OBJECT

    TestDbFixture db;
    bool originalFastEntry = false;
    bool originalCheckboxes = false;
    int firstId = -1;
    int secondId = -1;

private slots:
    void initTestCase() {
        db.init();
        originalFastEntry = Settings::instance().isFastEntryEnabled();
        originalCheckboxes = Settings::instance().isSelectionCheckboxesEnabled();
    }
    void cleanupTestCase() {
        Settings::instance().setFastEntryEnabled(originalFastEntry);
        Settings::instance().setSelectionCheckboxesEnabled(originalCheckboxes);
    }

    void init() {
        db.yankDb();
        db.seedDefaultUser();
        firstId = AccountDAO::createAccount(1, "First", "Checking", 0);
        secondId = AccountDAO::createAccount(1, "Second", "Checking", 0);
        QVERIFY(firstId > 0);
        QVERIFY(secondId > 0);
    }
    void cleanup() { db.close(); }

    void newTransactionUsesShownAccount() {
        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(secondId);

        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        QVERIFY(editor);
        QVERIFY(editor->isEditingNew());
        QCOMPARE(editor->findChild<QComboBox*>("accountCombo")->currentData().toInt(), secondId);
    }

    void savingWritesImmediatelyAndClosesEditor() {
        Settings::instance().setFastEntryEnabled(false);
        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(firstId);
        QSignalSpy changed(&page, &TransactionsPage::transactionsChanged);

        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        editor->findChild<QDoubleSpinBox*>("amountEdit")->setValue(12.5);
        editor->findChild<QLineEdit*>("payeeEdit")->setText("Bakery");
        editor->findChild<QPushButton*>("saveButton")->click();

        QCOMPARE(changed.count(), 1);
        QVERIFY(editor->isHidden());
        const QList<Transaction> saved = TransactionDAO().getTransactionsForAccount(firstId);
        QCOMPARE(saved.size(), 1);
        QCOMPARE(saved.first().getPayee(), QString("Bakery"));
        QCOMPARE(saved.first().getAmountCents(), Cents(1250));
        QCOMPARE(saved.first().getType(), QString("expense"));
    }

    void selectingOneTransactionOpensEditor() {
        Transaction transaction(-1, firstId, "Bakery", 420, QDate::currentDate(), "", 0, "expense");
        const int transactionId = TransactionDAO().addTransaction(transaction);
        QVERIFY(transactionId > 0);

        TransactionsPage page;
        page.setUserId(1);
        auto* list = page.findChild<QListView*>("transactionList");
        auto* editor = page.findChild<TransactionEditor*>();
        QVERIFY(list);

        const QModelIndex index = static_cast<TransactionListModel*>(list->model())->indexForTransaction(transactionId);
        QVERIFY(index.isValid());

        // Ticking (selection only) shows bulk actions, not the editor
        list->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
        QVERIFY(editor->isHidden());
        QVERIFY(!page.findChild<QFrame*>("bulkBar")->isHidden());

        // Opening the row (click / Enter) shows the editor instead
        QVERIFY(QMetaObject::invokeMethod(&page, "openEditor", Q_ARG(QModelIndex, index)));
        QVERIFY(!editor->isHidden());
        QVERIFY(page.findChild<QFrame*>("bulkBar")->isHidden());
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QCOMPARE(editor->editedTransactionId(), transactionId);
        QCOMPARE(editor->findChild<QLineEdit*>("payeeEdit")->text(), QString("Bakery"));
    }

    void plainClicksOpenButOnlyCtrlSelects() {
        Settings::instance().setSelectionCheckboxesEnabled(false);
        TransactionDAO dao;
        const int a = dao.addTransaction(Transaction(-1, firstId, "A", 100, QDate::currentDate(), "", 0, "expense"));
        const int b = dao.addTransaction(Transaction(-1, firstId, "B", 200, QDate::currentDate(), "", 0, "expense"));

        TransactionsPage page;
        page.resize(1000, 700);
        page.setUserId(1);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        auto* editor = page.findChild<TransactionEditor*>();
        const QPoint pointA = list->visualRect(model->indexForTransaction(a)).center();
        const QPoint pointB = list->visualRect(model->indexForTransaction(b)).center();

        // Plain click: opens the editor, selects nothing
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, pointA);
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QVERIFY(!editor->isHidden());
        QCOMPARE(editor->editedTransactionId(), a);

        // Plain drag across rows: still no selection
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, pointA);
        QMouseEvent move(QEvent::MouseMove, pointB, list->viewport()->mapToGlobal(pointB),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(list->viewport(), &move);
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, pointB);
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());

        // Ctrl+click: selects (and closes the editor for bulk actions)
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::ControlModifier, pointA);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::ControlModifier, pointB);
        QCOMPARE(list->selectionModel()->selectedIndexes().size(), 2);
        QVERIFY(editor->isHidden());

        // A plain click again clears the selection and opens just that row
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, pointB);
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QCOMPARE(editor->editedTransactionId(), b);
    }

    void selectingMultipleShowsBulkBarAndClosesEditor() {
        TransactionDAO dao;
        const int a = dao.addTransaction(Transaction(-1, firstId, "A", 100, QDate::currentDate(), "", 0, "expense"));
        const int b = dao.addTransaction(Transaction(-1, firstId, "B", 200, QDate::currentDate(), "", 0, "expense"));

        TransactionsPage page;
        page.setUserId(1);
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        auto* editor = page.findChild<TransactionEditor*>();

        QVERIFY(QMetaObject::invokeMethod(&page, "openEditor", Q_ARG(QModelIndex, model->indexForTransaction(a))));
        QVERIFY(!editor->isHidden());

        list->selectionModel()->select(model->indexForTransaction(b), QItemSelectionModel::Select);
        QVERIFY(editor->isHidden());
        QVERIFY(!page.findChild<QFrame*>("bulkBar")->isHidden());
    }

    void ctrlAndShiftOnDayHeaderSelectTheDay() {
        TransactionDAO dao;
        const QDate today = QDate::currentDate();
        const int a = dao.addTransaction(Transaction(-1, firstId, "A", 100, today, "", 0, "expense"));
        const int b = dao.addTransaction(Transaction(-1, firstId, "B", 200, today, "", 0, "expense"));
        const int c = dao.addTransaction(Transaction(-1, firstId, "C", 300, today.addDays(-5), "", 0, "expense"));

        TransactionsPage page;
        page.setUserId(1);
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        auto selectedIds = [&]() {
            QSet<int> ids;
            for (const QModelIndex& index : list->selectionModel()->selectedIndexes()) {
                ids.insert(index.data(TransactionListModel::TransactionIdRole).toInt());
            }
            return ids;
        };
        auto headerFor = [&](const QDate& date) {
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row);
                if (index.data(TransactionListModel::KindRole).toInt() == static_cast<int>(TransactionListModel::Kind::DayHeader)
                    && index.data(TransactionListModel::DateRole).toDate() == date) {
                    return index;
                }
            }
            return QModelIndex();
        };

        // Ctrl: select the whole day, a second Ctrl deselects it
        QVERIFY(QMetaObject::invokeMethod(&page, "selectDay", Q_ARG(QModelIndex, headerFor(today)), Q_ARG(bool, false)));
        QCOMPARE(selectedIds(), QSet<int>({a, b}));
        QVERIFY(QMetaObject::invokeMethod(&page, "selectDay", Q_ARG(QModelIndex, headerFor(today)), Q_ARG(bool, false)));
        QVERIFY(selectedIds().isEmpty());

        // A folded day unfolds when its transactions get selected
        model->toggleDay(today.addDays(-5));
        QVERIFY(QMetaObject::invokeMethod(&page, "selectDay", Q_ARG(QModelIndex, headerFor(today.addDays(-5))), Q_ARG(bool, false)));
        QVERIFY(!model->isDayCollapsed(today.addDays(-5)));
        QCOMPARE(selectedIds(), QSet<int>({c}));

        // Shift: range from the current transaction through the clicked day.
        // Today's rows are newest first (b above a), so a range starting at a covers a and c only.
        list->selectionModel()->setCurrentIndex(model->indexForTransaction(a), QItemSelectionModel::ClearAndSelect);
        QVERIFY(QMetaObject::invokeMethod(&page, "selectDay", Q_ARG(QModelIndex, headerFor(today.addDays(-5))), Q_ARG(bool, true)));
        QCOMPARE(selectedIds(), QSet<int>({a, c}));
    }

    void zeroAmountIsRejected() {
        TransactionsPage page;
        page.setUserId(1);
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        editor->findChild<QPushButton*>("saveButton")->click();
        QVERIFY(TransactionDAO().getTransactionsForAccount(firstId).isEmpty());
    }

    void fastEntryStartsTheNextTransaction() {
        Settings::instance().setFastEntryEnabled(true);
        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(secondId);

        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        editor->findChild<QDoubleSpinBox*>("amountEdit")->setValue(3.0);
        editor->findChild<QPushButton*>("saveButton")->click();

        QVERIFY(!editor->isHidden());
        QVERIFY(editor->isEditingNew());
        QCOMPARE(editor->findChild<QDoubleSpinBox*>("amountEdit")->value(), 0.0);
        QCOMPARE(editor->findChild<QComboBox*>("accountCombo")->currentData().toInt(), secondId);
        QCOMPARE(TransactionDAO().getTransactionsForAccount(secondId).size(), 1);
    }
};

QTEST_MAIN(TestTransactionsPage)
#include "test_transactionspage.moc"
