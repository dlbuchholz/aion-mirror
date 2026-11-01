#include <QtTest>
#include <QAbstractButton>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>
#include <QToolTip>

#include "account.h"
#include "budget.h"
#include "settings.h"
#include "tests/test_db_fixture.h"
#include "widgets/dashboardpage.h"

class TestDashboardPage : public QObject {
    Q_OBJECT
    TestDbFixture db;
    int giro = -1;
    int groceries = 0;
    int rent = 0;
    QDate today;

    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    int spend(Cents cents, int category, const char* payee = "Shop") {
        return TransactionDAO().addTransaction(Transaction(-1, giro, payee, cents, today, "", category, "expense"));
    }

    static QString text(DashboardPage& page, const char* name) { return page.findChild<QLabel*>(name)->text(); }
    static QString money(Cents cents, bool sign = false) { return Settings::instance().formatCurrency(cents, sign); }

    static QAbstractButton* row(DashboardPage& page, const char* objectName, const QString& name) {
        for (QAbstractButton* button : page.findChildren<QAbstractButton*>(objectName)) {
            if (button->accessibleName() == name) return button;
        }
        return nullptr;
    }

private slots:
    void initTestCase() { db.init(); }
    void init() {
        db.yankDb();
        QVERIFY(UserDAO().createUser(User(1, "alex", "Alex Example")));
        giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 0);
        groceries = categoryId("Groceries");
        rent = categoryId("Rent");
        today = QDate::currentDate();
    }
    void cleanup() { db.close(); }

    void greetsByFirstName() {
        DashboardPage page;
        page.setUserId(1);
        QVERIFY(text(page, "greetingLabel").endsWith(", Alex"));
        QVERIFY(text(page, "greetingLabel").startsWith("Good "));
        QVERIFY(text(page, "dateLabel").contains(QLocale(QLocale::English).toString(today, "dddd")));
    }

    void tilesShowThisMonth() {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "ACME", 200000, today, "", 0, "income")) > 0);
        spend(50000, rent);
        DashboardPage page;
        page.setUserId(1);
        QCOMPARE(text(page, "balanceValue"), money(150000));
        QCOMPARE(text(page, "balanceDetail"), QString("%1 this month").arg(money(150000, true)));
        QCOMPARE(text(page, "incomeValue"), money(200000));
        QCOMPARE(text(page, "expensesValue"), money(50000));
        QCOMPARE(text(page, "savedValue"), money(150000));
        QCOMPARE(text(page, "savedDetail"), QString("75% of income"));
        QCOMPARE(text(page, "averageValue"), money(50000 / today.day()));
    }

    static void moveMouse(QWidget* widget, QPoint position) {
        QMouseEvent event(QEvent::MouseMove, QPointF(position), widget->mapToGlobal(QPointF(position)),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    }

    void hoveringTheChartShowsAMonthsNumbers() {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "ACME", 200000, today, "", 0, "income")) > 0);
        spend(50000, rent);
        DashboardPage page;
        page.setUserId(1);
        page.setAttribute(Qt::WA_DontShowOnScreen);
        page.resize(1100, 800);
        page.show();
        QCoreApplication::processEvents();
        auto* chart = page.findChild<QWidget*>("cashFlowChart");
        const QImage plain = chart->grab().toImage();

        moveMouse(chart, QPoint(chart->width() - 10, chart->height() / 2));   // The current month is last
        QVERIFY2(QToolTip::text().contains("Income " + money(200000)), qPrintable(QToolTip::text()));
        QVERIFY(QToolTip::text().contains("Net " + money(150000, true)));
        QVERIFY(chart->grab().toImage() != plain);       // The month is highlighted

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(chart, &leave);
        QCOMPARE(chart->grab().toImage(), plain);

        moveMouse(chart, QPoint(chart->width() / 2, chart->height() + 100));  // Below the months
        QCOMPARE(chart->grab().toImage(), plain);
    }

    void anEmptyChartDraws() {
        DashboardPage page;
        page.setUserId(1);
        page.setAttribute(Qt::WA_DontShowOnScreen);
        page.resize(1100, 800);
        page.show();
        QCoreApplication::processEvents();
        QVERIFY(!page.findChild<QWidget*>("cashFlowChart")->grab().isNull());
    }

    void enterOpensARowLikeAClick() {
        spend(1250, groceries);
        DashboardPage page;
        page.setUserId(1);
        QSignalSpy requested(&page, &DashboardPage::categoryRequested);
        QAbstractButton* rowButton = nullptr;
        for (QAbstractButton* button : page.findChildren<QAbstractButton*>()) {
            if (button->accessibleName() == "Groceries") rowButton = button;
        }
        QVERIFY(rowButton);
        QTest::keyClick(rowButton, Qt::Key_Return);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested[0][0].toInt(), groceries);
        QTest::keyClick(rowButton, Qt::Key_A);           // Other keys do nothing
        QCOMPARE(requested.count(), 1);
    }

    void overspendingIsCalledOut() {
        spend(1000, rent);
        DashboardPage page;
        page.setUserId(1);
        QCOMPARE(text(page, "savedDetail"), QString("More spent than earned"));
    }

    void emptyCardsInvite() {
        DashboardPage page;
        page.setUserId(1);
        QVERIFY(page.findChild<QLabel*>("budgetEmptyLabel"));
        QVERIFY(page.findChild<QLabel*>("categoryEmptyLabel"));
        QVERIFY(page.findChild<QLabel*>("recentEmptyLabel"));
    }

    void rowsLightUpUnderTheMouse() {
        spend(1000, groceries);
        DashboardPage page;
        page.setUserId(1);
        QAbstractButton* category = row(page, "categoryRow", "Groceries");
        const QImage plain = category->grab().toImage();
        category->setAttribute(Qt::WA_UnderMouse);
        QVERIFY(category->grab().toImage() != plain);
    }

    void withoutAUserThereIsNothingToShow() {
        DashboardPage page;
        page.reload();
        QVERIFY(page.findChildren<QAbstractButton*>("categoryRow").isEmpty());
    }

    void categoryBarsCanLeaveOutTheColors() {
        const bool original = Settings::instance().isCategoryColorsInOverviewsEnabled();
        Settings::instance().setCategoryColorsInOverviewsEnabled(false);
        spend(1000, groceries);
        DashboardPage page;
        page.setUserId(1);
        Settings::instance().setCategoryColorsInOverviewsEnabled(original);
        QVERIFY(row(page, "categoryRow", "Groceries"));
    }

    void budgetsShowTheClosestToTheLimitFirst() {
        BudgetDAO budgets;
        const QDate month(today.year(), today.month(), 1);
        QVERIFY(budgets.setMonthlyLimit(1, groceries, month, 40000));
        QVERIFY(budgets.setMonthlyLimit(1, rent, month, 10000));
        QVERIFY(budgets.setMonthlyLimit(1, categoryId("Utilities"), month, 10000));
        QVERIFY(budgets.setMonthlyLimit(1, categoryId("Entertainment"), month, 10000));
        spend(12000, rent);
        spend(1000, groceries);

        DashboardPage page;
        page.setUserId(1);
        QSignalSpy budgetRequested(&page, &DashboardPage::budgetRequested);
        const QList<QAbstractButton*> rows = page.findChildren<QAbstractButton*>("budgetProgressRow");
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.first()->accessibleName(), QString("Rent"));
        QCOMPARE(rows.first()->accessibleDescription(), QString("%1 over").arg(money(2000)));
        QVERIFY(text(page, "budgetSummary").endsWith("1 more budget"));
        rows.first()->click();
        page.findChild<QToolButton*>("openBudgetLink")->click();
        QCOMPARE(budgetRequested.count(), 2);
    }

    void clickingACategoryOrTransactionAsksToShowIt() {
        const int transactionId = spend(3120, groceries, "Rewe");
        spend(500, 0, "Kiosk");
        DashboardPage page;
        page.setUserId(1);
        QSignalSpy categoryRequested(&page, &DashboardPage::categoryRequested);
        QSignalSpy transactionRequested(&page, &DashboardPage::transactionRequested);

        row(page, "categoryRow", "Groceries")->click();
        row(page, "categoryRow", "No category")->click();
        QCOMPARE(categoryRequested.count(), 2);
        QCOMPARE(categoryRequested[0][0].toInt(), groceries);
        QCOMPARE(categoryRequested[1][0].toInt(), 0);

        QAbstractButton* recent = row(page, "recentRow", "Rewe");
        QCOMPARE(recent->accessibleDescription(), money(-3120, true));
        recent->click();
        QCOMPARE(transactionRequested.count(), 1);
        QCOMPARE(transactionRequested[0][0].toInt(), transactionId);
    }

    void quickActionsAreForwarded() {
        DashboardPage page;
        page.setUserId(1);
        QSignalSpy add(&page, &DashboardPage::addTransactionRequested);
        QSignalSpy all(&page, &DashboardPage::transactionsRequested);
        page.findChild<QPushButton*>("dashboardAddButton")->click();
        page.findChild<QToolButton*>("allTransactionsLink")->click();
        QCOMPARE(add.count(), 1);
        QCOMPARE(all.count(), 1);
    }
};

QTEST_MAIN(TestDashboardPage)
#include "test_dashboardpage.moc"
