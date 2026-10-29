#include <QtTest>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QSqlQuery>
#include <QToolButton>

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

    // The rule hints show a sparkle as an icon (a ✦ in text made Windows search its fonts). A
    // missing SVG gives a blank pixmap, so look at the pixels.
    static bool drawsSomething(const QPixmap& pixmap) {
        const QImage image = pixmap.toImage();
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0) return true;
            }
        }
        return false;
    }

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

    void rulesSuggestTheCategoryOfNewTransactions() {
        Settings::instance().setCategorySuggestionEnabled(true);
        int groceries = -1;
        int rent = -1;
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == "Groceries") groceries = category.getCategoryId();
            if (category.getName() == "Rent") rent = category.getCategoryId();
        }
        Rule rule;
        rule.categoryId = groceries;
        rule.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::Contains, "rewe")};
        QVERIFY(RuleDAO(1).addRule(rule) > 0);

        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(firstId);
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        auto* category = editor->findChild<QComboBox*>("categoryCombo");
        auto* note = editor->findChild<QWidget*>("categorySuggestion");
        auto* payee = editor->findChild<QLineEdit*>("payeeEdit");

        payee->setText("REWE Markt");
        QCOMPARE(category->currentData().toInt(), groceries);
        QVERIFY(!note->isHidden());
        QCOMPARE(editor->findChild<QLabel*>("categorySuggestionLabel")->text(),
                 QString("From your rule: Payee contains \"rewe\""));
        QVERIFY(drawsSomething(editor->findChild<QLabel*>("categorySuggestionIcon")->pixmap()));
        payee->setText("Aldi");                          // No longer matches: the suggestion goes
        QVERIFY(category->currentData().toInt() <= 0);
        QVERIFY(note->isHidden());

        // Undo takes the suggestion back and stops further ones
        payee->setText("REWE");
        editor->findChild<QToolButton*>("undoSuggestionButton")->click();
        QVERIFY(category->currentData().toInt() <= 0);
        QVERIFY(note->isHidden());
        payee->setText("REWE Markt");
        QVERIFY(category->currentData().toInt() <= 0);

        // A category chosen by hand stays
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        payee->setText("REWE");
        category->setCurrentIndex(category->findData(rent));
        emit category->activated(category->currentIndex());
        payee->setText("REWE Markt");
        QCOMPARE(category->currentData().toInt(), rent);
        QVERIFY(note->isHidden());

        // Existing transactions keep their category
        const int id = TransactionDAO().addTransaction(Transaction(-1, firstId, "Kiosk", 300, QDate::currentDate(), "", rent, "expense"));
        editor->editTransaction(TransactionDAO().getTransactionById(id));
        payee->setText("REWE");
        QCOMPARE(category->currentData().toInt(), rent);

        // Turned off in Settings: no suggestions at all
        Settings::instance().setCategorySuggestionEnabled(false);
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        payee->setText("REWE Markt");
        QVERIFY(category->currentData().toInt() <= 0);
        QVERIFY(note->isHidden());
        Settings::instance().setCategorySuggestionEnabled(true);
    }

    void rulesOfferANewPayeeNameWithoutChangingIt() {
        Settings::instance().setFastEntryEnabled(false);
        Settings::instance().setCategorySuggestionEnabled(true);
        Rule rule;
        rule.newPayee = "Rewe";
        rule.conditions = {RuleCondition(RuleCondition::Field::Payee, RuleCondition::Operator::StartsWith, "rewe markt")};
        rule.id = RuleDAO(1).addRule(rule);
        QVERIFY(rule.id > 0);

        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(firstId);
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        auto* editor = page.findChild<TransactionEditor*>();
        auto* payee = editor->findChild<QLineEdit*>("payeeEdit");
        auto* offer = editor->findChild<QWidget*>("payeeRenameSuggestion");
        auto* original = editor->findChild<QLabel*>("originalPayeeLabel");

        payee->setText("REWE MARKT GMBH 4711");
        QVERIFY(!offer->isHidden());
        QCOMPARE(payee->text(), QString("REWE MARKT GMBH 4711"));    // Only offered
        QCOMPARE(editor->findChild<QLabel*>("payeeRenameLabel")->text(), QString("Your rule suggests the name \"Rewe\""));
        QVERIFY(drawsSomething(editor->findChild<QLabel*>("payeeRenameIcon")->pixmap()));

        editor->findChild<QToolButton*>("acceptRenameButton")->click();
        QCOMPARE(payee->text(), QString("Rewe"));
        QVERIFY(offer->isHidden());                      // Done; the rule still matches the original name
        QVERIFY(original->text().startsWith("Originally: REWE MARKT GMBH 4711 · renamed by a rule"));
        QVERIFY(!original->isHidden());

        editor->findChild<QDoubleSpinBox*>("amountEdit")->setValue(32.4);
        editor->findChild<QPushButton*>("saveButton")->click();
        const Transaction saved = TransactionDAO().getTransactionsForAccount(firstId).first();
        QCOMPARE(saved.getPayee(), QString("Rewe"));
        QCOMPARE(saved.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));
        QCOMPARE(saved.getRenamedByRuleId(), rule.id);

        // Opening it again shows the original, and offers nothing
        editor->editTransaction(saved);
        QVERIFY(original->text().startsWith("Originally: REWE MARKT GMBH 4711 · renamed by a rule"));
        QVERIFY(offer->isHidden());

        // Saved unchanged it stays the rule's name; typed by hand it's yours
        editor->findChild<QPushButton*>("saveButton")->click();
        QCOMPARE(TransactionDAO().getTransactionById(saved.getTransactionId()).getRenamedByRuleId(), rule.id);
        editor->editTransaction(TransactionDAO().getTransactionById(saved.getTransactionId()));
        payee->setText("Rewe Markt Hauptstraße");
        editor->findChild<QPushButton*>("saveButton")->click();
        const Transaction edited = TransactionDAO().getTransactionById(saved.getTransactionId());
        QCOMPARE(edited.getRenamedByRuleId(), -1);
        QCOMPARE(edited.getOriginalPayee(), QString("REWE MARKT GMBH 4711"));   // The bank's name stays
        editor->editTransaction(edited);
        QVERIFY(!original->text().contains("renamed by a rule"));

        // Turned off: nothing offered
        Settings::instance().setCategorySuggestionEnabled(false);
        QVERIFY(QMetaObject::invokeMethod(&page, "addTransaction"));
        QVERIFY(original->isHidden());
        payee->setText("REWE MARKT City");
        QVERIFY(offer->isHidden());
        Settings::instance().setCategorySuggestionEnabled(true);
    }

    void restoringGivesBackTheBanksName() {
        TransactionDAO dao;
        Transaction bankRow(-1, firstId, "AMZN Mktp DE*2K4LT0", 2399, QDate::currentDate(), "", 0, "expense");
        const QString bankHash = bankRow.calculateHash();
        bankRow.renamePayee("Amazon", -1);
        const int id = dao.addTransaction(bankRow);

        TransactionsPage page;
        page.setUserId(1);
        page.openTransaction(id);
        auto* editor = page.findChild<TransactionEditor*>();
        auto* original = editor->findChild<QLabel*>("originalPayeeLabel");
        QVERIFY(!original->isHidden());
        QVERIFY(original->text().contains("<a href=\"restore\">Restore</a>"));

        emit original->linkActivated("restore");
        QCOMPARE(editor->findChild<QLineEdit*>("payeeEdit")->text(), QString("AMZN Mktp DE*2K4LT0"));
        QVERIFY(original->isHidden());

        editor->findChild<QPushButton*>("saveButton")->click();
        const Transaction saved = dao.getTransactionById(id);
        QCOMPARE(saved.getPayee(), QString("AMZN Mktp DE*2K4LT0"));
        QVERIFY(saved.getOriginalPayee().isEmpty());     // No longer renamed
        QCOMPARE(saved.calculateHash(), bankHash);       // Imports still recognise it
        QCOMPARE(dao.getExistingHashes({bankHash}).size(), 1);
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

        // Right-clicking inside the selection keeps it, for the menu
        QTest::mouseClick(list->viewport(), Qt::RightButton, Qt::NoModifier, pointA);
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

    void switchingTheAccountKeepsTheSelectionBarRight() {
        TransactionDAO dao;
        const int a = dao.addTransaction(Transaction(-1, firstId, "A", 100, QDate::currentDate(), "", 0, "expense"));
        const int b = dao.addTransaction(Transaction(-1, secondId, "B", 200, QDate::currentDate(), "", 0, "expense"));

        TransactionsPage page;
        page.setUserId(1);
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        auto* bar = page.findChild<QFrame*>("bulkBar");
        list->selectionModel()->select(model->indexForTransaction(a), QItemSelectionModel::Select);
        list->selectionModel()->select(model->indexForTransaction(b), QItemSelectionModel::Select);
        QVERIFY(!bar->isHidden());

        // An account with one of them: that one stays selected
        page.showAccount(firstId);
        QCOMPARE(list->selectionModel()->selectedIndexes().size(), 1);
        QVERIFY(!bar->isHidden());
        QVERIFY(bar->findChild<QLabel*>()->text().startsWith("1 transaction selected"));

        // Clear selection hides the bar
        for (QToolButton* button : bar->findChildren<QToolButton*>()) {
            if (button->text() == "Clear selection") button->click();
        }
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QVERIFY(bar->isHidden());

        // An account with none of the selected ones: no bar
        page.showAccount(TransactionsPage::ALL_ACCOUNTS);
        list->selectionModel()->select(model->indexForTransaction(a), QItemSelectionModel::Select);
        page.showAccount(secondId);
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QVERIFY(bar->isHidden());
    }

    void searchingKeepsTheSelectionBarRight() {
        TransactionDAO dao;
        const int a = dao.addTransaction(Transaction(-1, firstId, "Apples", 100, QDate::currentDate(), "", 0, "expense"));
        const int b = dao.addTransaction(Transaction(-1, firstId, "Bread", 200, QDate::currentDate(), "", 0, "expense"));

        TransactionsPage page;
        page.setUserId(1);
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        auto* bar = page.findChild<QFrame*>("bulkBar");
        auto* search = page.findChild<QLineEdit*>("searchEdit");
        auto* label = bar->findChild<QLabel*>();
        list->selectionModel()->select(model->indexForTransaction(a), QItemSelectionModel::Select);
        list->selectionModel()->select(model->indexForTransaction(b), QItemSelectionModel::Select);

        // Hidden by the search: drops out of the selection, the bar counts what's left
        search->setText("Apples");
        QCOMPARE(list->selectionModel()->selectedIndexes().size(), 1);
        QVERIFY(label->text().startsWith("1 transaction selected"));

        // Clearing the search keeps the rest selected
        search->clear();
        QCOMPARE(list->selectionModel()->selectedIndexes().size(), 1);
        QVERIFY(!bar->isHidden());

        // Nothing left selected: no bar
        search->setText("nothing matches this");
        QVERIFY(list->selectionModel()->selectedIndexes().isEmpty());
        QVERIFY(bar->isHidden());
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

    void openTransactionClearsFiltersAndUnfoldsItsDay() {
        const QDate lastYear = QDate::currentDate().addYears(-1);
        const int transactionId = TransactionDAO().addTransaction(Transaction(-1, secondId, "Old shop", 990, lastYear, "", 0, "expense"));
        QVERIFY(transactionId > 0);
        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(firstId);                                // Hides it
        page.findChild<QLineEdit*>("searchEdit")->setText("something else");  // So does the search
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        model->toggleDay(lastYear);

        page.openTransaction(transactionId);
        auto* editor = page.findChild<TransactionEditor*>();
        QVERIFY(!editor->isHidden());
        QCOMPARE(editor->editedTransactionId(), transactionId);
        QVERIFY(!model->isDayCollapsed(lastYear));
        QVERIFY(page.findChild<QLineEdit*>("searchEdit")->text().isEmpty());
    }

    void showCategoryThisMonthFiltersTheList() {
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO dao;
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Rewe", 1000, today, "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, secondId, "Aldi", 2000, today, "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Kiosk", 300, today, "", 0, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Rewe", 500, today.addMonths(-1), "", groceries, "expense")) > 0);

        TransactionsPage page;
        page.setUserId(1);
        page.showAccount(firstId);
        auto* model = static_cast<TransactionListModel*>(page.findChild<QListView*>("transactionList")->model());

        page.showCategoryThisMonth(groceries);
        QCOMPARE(model->totals().count, 2);                       // Both accounts, this month only
        QCOMPARE(page.findChild<QComboBox*>("categoryFilter")->currentData().toInt(), groceries);
        page.showCategoryThisMonth(TransactionFilter::NO_CATEGORY);
        QCOMPARE(model->totals().count, 1);
    }

    void showCategoryInRangeShowsTheDaysAsAChip() {
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        TransactionDAO dao;
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Rewe", 1000, QDate(2026, 5, 4), "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Rewe", 1000, QDate(2026, 6, 30), "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, secondId, "Aldi", 2000, QDate(2026, 6, 1), "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Rewe", 500, QDate(2026, 7, 1), "", groceries, "expense")) > 0);
        QVERIFY(dao.addTransaction(Transaction(-1, firstId, "Kiosk", 300, QDate(2026, 6, 2), "", 0, "expense")) > 0);

        TransactionsPage page;
        page.setUserId(1);
        auto* model = static_cast<TransactionListModel*>(page.findChild<QListView*>("transactionList")->model());
        auto* chip = page.findChild<QToolButton*>("rangeChip");
        QVERIFY(chip->isHidden());

        page.showCategoryInRange(groceries, firstId, QDate(2026, 5, 1), QDate(2026, 6, 30));
        QCOMPARE(model->totals().count, 2);                       // That account, those days
        QVERIFY(!chip->isHidden());
        QVERIFY(chip->isChecked());
        QCOMPARE(chip->text(), QString("May – June 2026"));

        page.showCategoryInRange(0, TransactionsPage::ALL_ACCOUNTS, QDate(2026, 6, 1), QDate(2026, 6, 30));
        QCOMPARE(model->totals().count, 1);                       // 0 is no category

        // Another period leaves the days
        const QList<QToolButton*> chips = page.findChildren<QToolButton*>();
        for (QToolButton* other : chips) {
            if (other->text() == "All time") other->click();
        }
        QVERIFY(chip->isHidden());
        QCOMPARE(model->totals().count, 1);                       // Still no category, all time
    }

    // --- Deleting and bulk changes ---

    int addExpense(const char* payee, Cents cents) {
        return TransactionDAO().addTransaction(Transaction(-1, firstId, payee, cents, QDate::currentDate(), "", 0, "expense"));
    }

    static void selectTransactions(TransactionsPage& page, const QList<int>& transactionIds) {
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        for (int transactionId : transactionIds) {
            list->selectionModel()->select(model->indexForTransaction(transactionId), QItemSelectionModel::Select);
        }
    }

    void bulkChangesStayQuick() {
        // Each change written to disk on its own took about 6 ms, so 1000 took seconds. Measured
        // against 25 single deletes, the old way, so a slow disk or build doesn't matter.
        QSqlDatabase database = DatabaseManager::instance().database();
        QVERIFY(database.transaction());
        QList<int> ids;
        for (int i = 0; i < 2025; ++i) {
            ids.append(TransactionDAO().addTransaction(Transaction(-1, firstId, QString("Payee %1").arg(i), 100 + i,
                                                                   QDate(2025, 1, 1).addDays(i % 600), "", 0, "expense")));
        }
        QVERIFY(database.commit());
        QElapsedTimer timer;
        timer.start();
        for (int id : ids.mid(2000)) QVERIFY(TransactionDAO().deleteTransaction(id));
        const qint64 oneByOne = timer.restart();

        TransactionsPage page;
        page.setUserId(1);
        page.confirm = [](const QString&, const QString&) { return true; };
        auto* list = page.findChild<QListView*>("transactionList");
        auto* model = static_cast<TransactionListModel*>(list->model());
        const auto selectAll = [&](const QList<int>& chosen) {
            QItemSelection selection;
            for (int id : chosen) {
                const QModelIndex index = model->indexForTransaction(id);
                selection.select(index, index);
            }
            list->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        };
        QSqlQuery query(database);
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();

        selectAll(ids.mid(0, 1000));
        timer.restart();
        for (QAction* candidate : page.findChild<QToolButton*>("bulkCategoryButton")->menu()->actions()) {
            if (candidate->data().toInt() == groceries || candidate->text() == "Groceries") {
                candidate->trigger();
                break;
            }
        }
        const qint64 categorising = timer.restart();
        selectAll(ids.mid(0, 1000));
        timer.restart();
        QVERIFY(QMetaObject::invokeMethod(&page, "deleteSelected"));
        const qint64 deleting = timer.restart();

        const QList<Transaction> left = TransactionDAO().getTransactionsForAccount(firstId);
        QCOMPARE(left.size(), 1000);
        QCOMPARE(left.first().getCategoryId(), 0);           // The categorised ones were the deleted ones
        const QByteArray times = QString("25 one by one %1 ms, categorising 1000 %2 ms, deleting 1000 %3 ms")
                                     .arg(oneByOne).arg(categorising).arg(deleting).toUtf8();
        QVERIFY2(categorising < oneByOne * 10, times.constData());
        QVERIFY2(deleting < oneByOne * 10, times.constData());
    }

    void aFailedBulkDeleteChangesNothing() {
        const int first = addExpense("A", 100);
        const int second = addExpense("B", 200);
        QVERIFY(!TransactionDAO().deleteTransactions({first, 9999, second}));   // 9999 doesn't exist
        QCOMPARE(TransactionDAO().getTransactionsForAccount(firstId).size(), 2);
        QVERIFY(TransactionDAO().deleteTransactions({first, second}));
        QVERIFY(TransactionDAO().getTransactionsForAccount(firstId).isEmpty());
    }

    void deletingAsksFirst() {
        const int transactionId = addExpense("Bakery", 420);
        TransactionsPage page;
        page.setUserId(1);
        QSignalSpy changed(&page, &TransactionsPage::transactionsChanged);
        QString asked;
        bool answer = false;
        page.confirm = [&](const QString&, const QString& text) { asked = text; return answer; };
        page.openTransaction(transactionId);
        auto* editor = page.findChild<TransactionEditor*>();

        emit editor->deleteRequested(transactionId);                            // "Delete" in the editor
        QCOMPARE(asked, QString("Delete this transaction? This can't be undone."));
        QCOMPARE(TransactionDAO().getTransactionsForAccount(firstId).size(), 1); // Said no
        QCOMPARE(changed.count(), 0);

        answer = true;
        emit editor->deleteRequested(transactionId);
        QVERIFY(TransactionDAO().getTransactionsForAccount(firstId).isEmpty());
        QVERIFY(editor->isHidden());
        QCOMPARE(changed.count(), 1);
    }

    void deletesTheSelection() {
        const int first = addExpense("A", 100);
        const int second = addExpense("B", 200);
        const int kept = addExpense("C", 300);
        TransactionsPage page;
        page.setUserId(1);
        QString asked;
        page.confirm = [&](const QString&, const QString& text) { asked = text; return true; };
        selectTransactions(page, {first, second});
        QVERIFY(QMetaObject::invokeMethod(&page, "deleteSelected"));
        QCOMPARE(asked, QString("Delete 2 transactions? This can't be undone."));
        const QList<Transaction> left = TransactionDAO().getTransactionsForAccount(firstId);
        QCOMPARE(left.size(), 1);
        QCOMPARE(left.first().getTransactionId(), kept);
        QVERIFY(page.findChild<QFrame*>("bulkBar")->isHidden());                // Nothing selected any more
    }

    void setsTheCategoryOfTheSelection() {
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const int first = addExpense("A", 100);
        const int second = addExpense("B", 200);
        const int untouched = addExpense("C", 300);
        TransactionsPage page;
        page.setUserId(1);
        selectTransactions(page, {first, second});

        QAction* action = nullptr;
        for (QAction* candidate : page.findChild<QToolButton*>("bulkCategoryButton")->menu()->actions()) {
            if (candidate->text() == "Groceries") action = candidate;
        }
        QVERIFY(action);
        action->trigger();
        for (const Transaction& transaction : TransactionDAO().getTransactionsForAccount(firstId)) {
            QCOMPARE(transaction.getCategoryId(), transaction.getTransactionId() == untouched ? 0 : groceries);
        }
    }
};

QTEST_MAIN(TestTransactionsPage)
#include "test_transactionspage.moc"
