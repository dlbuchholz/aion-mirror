#include <QtTest>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

#include "account.h"
#include "category.h"
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
        for (const Category& category : CategoryDAO().getAllCategories()) {
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

    void addsACategoryWithAColour() {
        CategoriesSettings settings;
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

    void rejectsDuplicateAndEmptyNames() {
        CategoriesSettings settings;
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
        CategoryDAO dao;
        const int from = dao.addCategory(Category(-1, "Lebensmittel", "expense"));
        CategoriesSettings settings;
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
        CategoryDAO dao;
        dao.addCategory(Category(-1, "Travel", "expense"));
        CategoriesSettings settings;
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

    void searchFiltersTheList() {
        CategoriesSettings settings;
        settings.findChild<QLineEdit*>("categorySearch")->setText("gro");
        settle();
        QVERIFY(row(settings, "Groceries"));
        QVERIFY(!row(settings, "Rent"));
    }

    void accountTypesCanBeAddedRenamedAndDeletedWhenUnused() {
        AccountDAO::createAccount(1, "Giro", "Checking", 0);
        AccountTypesSettings settings;
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
        QVERIFY(AccountDAO().getAccountTypes().contains("Card"));

        for (QWidget* typeRow : settings.findChildren<QWidget*>("accountTypeRow")) {
            if (typeRow->isVisibleTo(&settings) && typeRow->accessibleName() == "Card") {
                typeRow->findChild<QToolButton*>("deleteAccountTypeButton")->click();
            }
        }
        settle();
        QVERIFY(!AccountDAO().getAccountTypes().contains("Card"));
    }
};

QTEST_MAIN(TestCategorySettings)
#include "test_categorysettings.moc"
