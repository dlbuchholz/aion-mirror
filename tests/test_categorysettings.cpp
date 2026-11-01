#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

#include "account.h"
#include "budget.h"
#include "category.h"
#include "categorycolors.h"
#include "tests/test_db_fixture.h"
#include "widgets/accounttypessettings.h"
#include "widgets/categoriessettings.h"

class TestCategorySettings : public QObject {
    Q_OBJECT
    TestDbFixture db;

    static QAbstractButton* row(QWidget& widget, const QString& name) {
        for (QAbstractButton* button : widget.findChildren<QAbstractButton*>("categoryRow")) {
            if (button->accessibleName() == name && button->isVisibleTo(&widget)) return button;
        }
        return nullptr;
    }

    static Category named(const QString& name) {
        for (const Category& category : CategoryDAO(1).getAllCategories()) {
            if (category.getName() == name) return category;
        }
        return Category(-1, "", "");
    }

    static void settle() { QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); }

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); db.seedDefaultUser(); }
    void cleanup() { db.close(); }

    void listsCategoriesInGroupsWithBuiltInOnesLocked() {
        CategoriesSettings settings;
        settings.setUserId(1);
        QVERIFY(row(settings, "Groceries"));
        QVERIFY(row(settings, "Salary"));
        QAbstractButton* builtIn = row(settings, "Balance correction");
        QVERIFY(builtIn);
        builtIn->click();
        QVERIFY(!settings.findChild<QFrame*>("categoryEditor")->isVisibleTo(&settings));   // Not editable
        QStringList groups;
        for (QLabel* label : settings.findChildren<QLabel*>("categoryGroupLabel")) groups << label->text();
        QCOMPARE(groups, QStringList({"Expenses", "Income", "Built in"}));
    }

    void rowsLightUpWhenHoveredOrOpen() {
        CategoriesSettings settings;
        settings.setUserId(1);
        QAbstractButton* groceries = row(settings, "Groceries");
        const QImage plain = groceries->grab().toImage();
        groceries->setAttribute(Qt::WA_UnderMouse);
        QVERIFY(groceries->grab().toImage() != plain);
    }

    void theOpenCategoryCanBeClosedOrDeletedMeanwhile() {
        const int unused = CategoryDAO(1).addCategory(Category(-1, "Unused", "expense"));
        QVERIFY(unused > 0);
        CategoriesSettings settings;
        settings.setUserId(1);
        auto* editor = settings.findChild<QFrame*>("categoryEditor");
        row(settings, "Unused")->click();
        settle();
        QVERIFY(editor->isVisibleTo(&settings));
        for (QPushButton* button : editor->findChildren<QPushButton*>()) {
            if (button->text() == "Cancel") {
                button->click();
                break;
            }
        }
        settle();
        QVERIFY(!editor->isVisibleTo(&settings));

        row(settings, "Unused")->click();
        settle();
        QVERIFY(CategoryDAO(1).deleteCategory(unused));
        settings.reload();
        settle();
        QVERIFY(!row(settings, "Unused"));
        QVERIFY(!editor->isVisibleTo(&settings));
    }

    void removingAsksByDefault() {
        CategoriesSettings settings;
        QTimer::singleShot(0, qApp, []() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            box->button(QMessageBox::Yes)->click();
        });
        QVERIFY(settings.confirm("Delete", "Sure?"));
    }

    void openingACategoryKeepsTheListWhereItIs() {
        CategoryDAO dao(1);
        for (int i = 0; i < 25; ++i) {
            QVERIFY(dao.addCategory(Category(-1, QString("Extra %1").arg(i, 2, 10, QChar('0')), "expense")) > 0);
        }
        CategoriesSettings settings;
        settings.setUserId(1);
        settings.resize(600, 400);
        settings.show();
        settings.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&settings));
        auto* scrollBar = settings.findChild<QScrollArea*>()->verticalScrollBar();
        QVERIFY(scrollBar->maximum() > 0);
        scrollBar->setValue(scrollBar->maximum() / 2);
        const int scrolled = scrollBar->value();

        // A row in view, clicked like with the mouse (which also gives it the focus)
        QAbstractButton* clicked = nullptr;
        QWidget* viewport = settings.findChild<QScrollArea*>()->viewport();
        for (QAbstractButton* candidate : settings.findChildren<QAbstractButton*>("categoryRow")) {
            const QPoint top = candidate->mapTo(viewport, QPoint(0, 0));
            if (candidate->isVisible() && top.y() > 40 && top.y() < viewport->height() - 80) {
                clicked = candidate;
                break;
            }
        }
        QVERIFY(clicked);
        QTest::mouseClick(clicked, Qt::LeftButton);
        settle();
        QVERIFY(settings.findChild<QFrame*>("categoryEditor")->isVisible());
        QCOMPARE(scrollBar->value(), scrolled);
    }

    void addsACategoryWithAColour() {
        CategoriesSettings settings;
        settings.setUserId(1);
        QSignalSpy changed(&settings, &CategoriesSettings::categoriesChanged);
        settings.findChild<QPushButton*>("addCategoryButton")->click();
        settings.findChild<QLineEdit*>("categoryNameEdit")->setText("  Travel  ");
        settings.findChild<QAbstractButton*>("swatch_teal")->click();
        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        settle();

        QCOMPARE(changed.count(), 1);
        const Category travel = named("Travel");
        QVERIFY(travel.getCategoryId() > 0);
        QCOMPARE(travel.getColor(), QString("teal"));
        QCOMPARE(travel.getType(), QString("expense"));
        QVERIFY(row(settings, "Travel"));
    }

    void eighteenColoursInHueOrderAndTwoRows() {
        const QStringList names = CategoryColors::names();
        QCOMPARE(names, (QStringList{"red", "coral", "orange", "amber", "yellow", "lime", "green", "olive", "teal",
                                     "sky", "blue", "indigo", "purple", "magenta", "pink", "brown", "slate", "gray"}));
        // No two solid tones (dots, bars, the picker) so close they'd pass for each other
        for (int i = 0; i < names.size(); ++i) {
            for (int j = i + 1; j < names.size(); ++j) {
                const QColor a = CategoryColors::swatch(names[i]).solid;
                const QColor b = CategoryColors::swatch(names[j]).solid;
                const double distance = std::hypot(a.red() - b.red(), a.green() - b.green(), a.blue() - b.blue());
                QVERIFY2(distance > 30, qPrintable(QString("%1 and %2 look alike").arg(names[i], names[j])));
            }
        }

        CategoriesSettings settings;
        settings.resize(700, 600);
        settings.setUserId(1);
        settings.show();
        settings.findChild<QPushButton*>("addCategoryButton")->click();
        QCoreApplication::processEvents();
        // No colour and nine colours in the first row, the other nine below
        const auto position = [&settings](const char* name) {
            return settings.findChild<QAbstractButton*>(name)->mapTo(&settings, QPoint(0, 0));
        };
        QCOMPARE(position("swatch_teal").y(), position("swatch_none").y());
        QVERIFY(position("swatch_sky").y() > position("swatch_teal").y());
        QCOMPARE(position("swatch_sky").x(), position("swatch_none").x());

        settings.findChild<QLineEdit*>("categoryNameEdit")->setText("Hobby");
        settings.findChild<QAbstractButton*>("swatch_indigo")->click();        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        settle();
        QCOMPARE(named("Hobby").getColor(), QString("indigo"));
    }

    void previewShowsTheTagInBothThemes() {
        CategoriesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addCategoryButton")->click();
        // Each theme's patch holds the tag; its accessible description is the colour's name
        const auto preview = [&settings](const char* theme) {
            for (QWidget* child : settings.findChild<QWidget*>(theme)->findChildren<QWidget*>()) {
                if (!child->accessibleName().isEmpty()) return QStringList{child->accessibleName(), child->accessibleDescription()};
            }
            return QStringList();
        };
        QCOMPARE(preview("lightThemePreview"), (QStringList{"Name", "No colour"}));   // Nothing typed yet

        settings.findChild<QLineEdit*>("categoryNameEdit")->setText("Hobby");
        auto* teal = settings.findChild<QAbstractButton*>("swatch_teal");
        teal->click();
        QCOMPARE(preview("lightThemePreview"), (QStringList{"Hobby", "Teal"}));
        QCOMPARE(preview("darkThemePreview"), (QStringList{"Hobby", "Teal"}));
        // Hovering another colour previews it without choosing it
        auto* red = settings.findChild<QAbstractButton*>("swatch_red");
        QEnterEvent enter(QPointF(5, 5), QPointF(5, 5), QPointF(5, 5));
        QCoreApplication::sendEvent(red, &enter);
        QCOMPARE(preview("darkThemePreview"), (QStringList{"Hobby", "Red"}));
        QVERIFY(teal->isChecked());
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(red, &leave);
        QCOMPARE(preview("darkThemePreview"), (QStringList{"Hobby", "Teal"}));

        // An existing category opens with its own name and colour
        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        settle();
        row(settings, "Hobby")->click();
        QCOMPARE(preview("lightThemePreview"), (QStringList{"Hobby", "Teal"}));
    }

    void rejectsDuplicateAndEmptyNames() {
        CategoriesSettings settings;
        settings.setUserId(1);
        settings.findChild<QPushButton*>("addCategoryButton")->click();
        auto* name = settings.findChild<QLineEdit*>("categoryNameEdit");
        auto* error = settings.findChild<QLabel*>("categoryError");
        name->setText("groceries");
        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        QVERIFY(error->isVisibleTo(&settings));
        QVERIFY(error->text().contains("Merge"));
        name->setText(" ");
        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        QCOMPARE(error->text(), QString("Enter a name."));
    }

    void editsNameTypeAndColour() {
        CategoriesSettings settings;
        settings.setUserId(1);
        row(settings, "Entertainment")->click();
        settle();
        QCOMPARE(settings.findChild<QLineEdit*>("categoryNameEdit")->text(), QString("Entertainment"));
        settings.findChild<QLineEdit*>("categoryNameEdit")->setText("Fun");
        settings.findChild<QAbstractButton*>("swatch_purple")->click();
        settings.findChild<QPushButton*>("saveCategoryButton")->click();
        settle();
        QCOMPARE(named("Fun").getColor(), QString("purple"));
        QCOMPARE(named("Entertainment").getCategoryId(), -1);
    }

    void mergesAfterConfirming() {
        CategoryDAO dao(1);
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        CategoriesSettings settings;
        settings.setUserId(1);
        QString asked;
        settings.confirm = [&asked](const QString&, const QString& text) { asked = text; return true; };
        row(settings, "Lebensmittel")->click();
        settle();

        QMenu* menu = settings.findChild<QToolButton*>("mergeCategoryButton")->menu();
        QAction* groceries = nullptr;
        for (QAction* action : menu->actions()) {
            if (action->text() == "Groceries") groceries = action;
            QVERIFY(action->text() != "Lebensmittel" && action->text() != "Balance correction");
        }
        QVERIFY(groceries);
        groceries->trigger();
        settle();
        QVERIFY(asked.contains("Merge \"Lebensmittel\" into \"Groceries\""));
        QCOMPARE(dao.getCategoryById(from).getCategoryId(), -1);
        QVERIFY(!row(settings, "Lebensmittel"));
    }

    void deleteCanBeCancelled() {
        CategoryDAO dao(1);
        dao.addCategory(Category(-1, "Travel", "expense"));
        CategoriesSettings settings;
        settings.setUserId(1);
        settings.confirm = [](const QString&, const QString&) { return false; };
        row(settings, "Travel")->click();
        settle();
        settings.findChild<QPushButton*>("deleteCategoryButton")->click();
        QVERIFY(named("Travel").getCategoryId() > 0);

        settings.confirm = [](const QString&, const QString&) { return true; };
        settings.findChild<QPushButton*>("deleteCategoryButton")->click();
        settle();
        QCOMPARE(named("Travel").getCategoryId(), -1);
    }

    static QAction* mergeEntry(CategoriesSettings& settings, const QString& name) {
        for (QAction* action : settings.findChild<QToolButton*>("mergeCategoryButton")->menu()->actions()) {
            if (action->text() == name) return action;
        }
        return nullptr;
    }

    void mergingSaysWhatHappensToBudgets() {
        CategoryDAO dao(1);
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        const QDate month(2026, 10, 1);
        QVERIFY(BudgetDAO().setMonthlyLimit(1, from, month, 1000));
        CategoriesSettings settings;
        settings.setUserId(1);
        QString asked;
        settings.confirm = [&asked](const QString&, const QString& text) { asked = text; return false; };
        row(settings, "Lebensmittel")->click();
        settle();

        mergeEntry(settings, "Rent")->trigger();
        QVERIFY(asked.contains("Its budgets move along."));
        QVERIFY(dao.getCategoryById(from).getCategoryId() > 0);     // Said no

        QVERIFY(BudgetDAO().setMonthlyLimit(1, named("Groceries").getCategoryId(), month, 2000));
        settings.reload();                               // Stays open
        settle();
        mergeEntry(settings, "Groceries")->trigger();
        QVERIFY(asked.contains("Its budgets are removed, \"Groceries\" keeps its own."));
    }

    void whatCantBeMergedDeletedOrSavedIsSaid() {
        CategoryDAO dao(1);
        const int travel = dao.addCategory(Category(-1, "Travel", "expense"));
        const int account = AccountDAO::createAccount(1, "Giro", "Checking", 0);
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, account, "DB", 100, QDate(2026, 10, 1), "", travel, "expense")) > 0);
        CategoriesSettings settings;
        settings.setUserId(1);
        QString asked;
        settings.confirm = [&asked](const QString&, const QString& text) { asked = text; return true; };
        auto* error = settings.findChild<QLabel*>("categoryError");
        row(settings, "Travel")->click();
        settle();

        QVERIFY(TestDbFixture::refuse("DELETE", "categories"));
        mergeEntry(settings, "Groceries")->trigger();
        QCOMPARE(error->text(), QString("The categories couldn't be merged."));

        settings.findChild<QPushButton*>("deleteCategoryButton")->click();
        QVERIFY(asked.contains("It's used by"));
        QCOMPARE(error->text(), QString("The category couldn't be deleted."));
        QCOMPARE(dao.getCategoryById(travel).getCategoryId(), travel);

        QVERIFY(TestDbFixture::refuse("UPDATE", "categories"));
        settings.findChild<QLineEdit*>("categoryNameEdit")->setText("Reisen");
        for (QPushButton* button : settings.findChild<QFrame*>("categoryEditor")->findChildren<QPushButton*>()) {
            if (button->text() == "Save") button->click();
        }
        QCOMPARE(error->text(), QString("The category couldn't be saved."));
    }

    void accountTypesSayWhatWentWrong() {
        AccountTypesSettings settings;
        settings.setUserId(1);
        auto* error = settings.findChild<QLabel*>("accountTypeError");
        auto* add = settings.findChild<QPushButton*>("addAccountTypeButton");
        auto* newType = settings.findChild<QLineEdit*>("newAccountTypeEdit");

        add->click();
        QCOMPARE(error->text(), QString("Enter a name for the new account type."));
        newType->setText("Savings");
        add->click();
        QVERIFY(error->text().startsWith("\"Savings\" couldn't be added."));

        const auto typeRow = [&settings](const QString& name) -> QWidget* {
            for (QWidget* candidate : settings.findChildren<QWidget*>("accountTypeRow")) {
                if (candidate->isVisibleTo(&settings) && candidate->accessibleName() == name) return candidate;
            }
            return nullptr;
        };
        auto* name = typeRow("Virtual")->findChild<QLineEdit*>("accountTypeName");
        emit name->editingFinished();                    // Unchanged: nothing happens
        name->setText("  ");
        emit name->editingFinished();
        QCOMPARE(error->text(), QString("An account type needs a name."));
        QCOMPARE(name->text(), QString("Virtual"));      // Back to what it was

        name->setText("Savings");
        emit name->editingFinished();
        QVERIFY(error->text().startsWith("\"Savings\" couldn't be saved."));

        QVERIFY(TestDbFixture::refuse("DELETE", "account_types"));
        typeRow("Virtual")->findChild<QToolButton*>("deleteAccountTypeButton")->click();
        QCOMPARE(error->text(), QString("The account type couldn't be deleted."));
    }

    void searchFiltersTheList() {
        CategoriesSettings settings;
        settings.setUserId(1);
        settings.findChild<QLineEdit*>("categorySearch")->setText("gro");
        settle();
        QVERIFY(row(settings, "Groceries"));
        QVERIFY(!row(settings, "Rent"));
    }

    void accountTypesCanBeAddedRenamedAndDeletedWhenUnused() {
        AccountDAO::createAccount(1, "Giro", "Checking", 0);
        AccountTypesSettings settings;
        settings.setUserId(1);
        QSignalSpy changed(&settings, &AccountTypesSettings::accountTypesChanged);
        settings.findChild<QLineEdit*>("newAccountTypeEdit")->setText("Credit card");
        settings.findChild<QPushButton*>("addAccountTypeButton")->click();
        settle();
        QCOMPARE(changed.count(), 1);

        QWidget* creditCard = nullptr;
        QWidget* checking = nullptr;
        for (QWidget* typeRow : settings.findChildren<QWidget*>("accountTypeRow")) {
            if (!typeRow->isVisibleTo(&settings)) continue;
            if (typeRow->accessibleName() == "Credit card") creditCard = typeRow;
            if (typeRow->accessibleName() == "Checking") checking = typeRow;
        }
        QVERIFY(creditCard && checking);
        QVERIFY(!checking->findChild<QToolButton*>("deleteAccountTypeButton")->isEnabled());   // Used by "Giro"

        auto* name = creditCard->findChild<QLineEdit*>("accountTypeName");
        name->setText("Card");
        emit name->editingFinished();
        settle();
        QVERIFY(AccountDAO().getAccountTypes(1).contains("Card"));

        for (QWidget* typeRow : settings.findChildren<QWidget*>("accountTypeRow")) {
            if (typeRow->isVisibleTo(&settings) && typeRow->accessibleName() == "Card") {
                typeRow->findChild<QToolButton*>("deleteAccountTypeButton")->click();
            }
        }
        settle();
        QVERIFY(!AccountDAO().getAccountTypes(1).contains("Card"));
    }

    void showsOnlyTheUsersOwnCategoriesAndAccountTypes() {
        QVERIFY(UserDAO().createUser(User(2, "sam", "Sam")));
        QVERIFY(CategoryDAO(2).addCategory(Category(-1, "Sam's hobby", "expense")) > 0);
        QVERIFY(AccountDAO().addAccountType(2, "Sam's depot") > 0);

        CategoriesSettings categories;
        categories.setUserId(1);
        QVERIFY(row(categories, "Groceries"));
        QVERIFY(!row(categories, "Sam's hobby"));
        categories.setUserId(2);
        settle();
        QVERIFY(row(categories, "Sam's hobby"));

        AccountTypesSettings types;
        types.setUserId(1);
        settle();
        for (QWidget* typeRow : types.findChildren<QWidget*>("accountTypeRow")) {
            QVERIFY(typeRow->accessibleName() != "Sam's depot");
        }
    }
};

QTEST_MAIN(TestCategorySettings)
#include "test_categorysettings.moc"
