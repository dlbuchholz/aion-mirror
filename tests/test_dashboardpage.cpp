#include <QtTest>
#include <QAbstractButton>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QToolButton>
#include <QToolTip>

#include "applocale.h"
#include "account.h"
#include "budget.h"
#include "settings.h"
#include "theme.h"
#include "tests/test_db_fixture.h"
#include "widgets/dashboardpage.h"
#include "widgets/wraplabel.h"

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

    void tilesCountTransactionsInTheSingularToo() {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "ACME", 200000, today, "", 0, "income")) > 0);
        spend(500, rent);
        spend(700, groceries);
        DashboardPage page;
        page.setUserId(1);
        const QString month = AppLocale::date(today, AppLocale::DateFormat::Month);
        QCOMPARE(text(page, "incomeDetail"), QString("1 transaction in %1").arg(month));
        // Nothing spent last month to compare with: counted like the income
        QCOMPARE(text(page, "expensesDetail"), QString("2 transactions in %1").arg(month));
    }

    void saysHowManyDaysAreLeft() {
        QCOMPARE(DashboardPage::daysLeftText(0), QString("Last day of the month"));
        QCOMPARE(DashboardPage::daysLeftText(1), QString("1 day left this month"));
        QCOMPARE(DashboardPage::daysLeftText(12), QString("12 days left this month"));
        DashboardPage page;
        page.setUserId(1);
        QVERIFY(text(page, "dateLabel").endsWith(DashboardPage::daysLeftText(today.daysInMonth() - today.day())));
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

    static QList<QFrame*> tiles(DashboardPage& page) {
        QList<QFrame*> found;
        for (const char* name : {"balanceTile", "incomeTile", "expensesTile", "savedTile", "averageTile"}) {
            found << page.findChild<QFrame*>(name);
        }
        return found;
    }

    // How wide the page is with the tiles' texts each on one line, side by side. Measured, as
    // fonts differ: the offscreen platform's is much wider than Windows'.
    static int pageWidthForOneLineTiles(DashboardPage& page) {
        const QList<QFrame*> all = tiles(page);
        int width = 0;
        for (QFrame* tile : all) width += tile->sizeHint().width();
        const QWidget* content = page.findChild<QScrollArea*>()->widget();
        const QMargins margins = content->layout()->contentsMargins();
        return width + 10 * int(all.size() - 1) + margins.left() + margins.right();
    }

    static void show(DashboardPage& page, int width) {
        page.resize(width, 2000);         // High enough not to scroll down, so no scroll bar takes width
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
    }

    void tilesAreAsWideAsEachOther() {
        DashboardPage page;
        page.setUserId(1);
        show(page, pageWidthForOneLineTiles(page) + 200);
        const QList<QFrame*> all = tiles(page);
        for (QFrame* tile : all) {
            QVERIFY(qAbs(tile->width() - all.first()->width()) <= 1);
        }
        // With room, the texts stay on one line
        for (const char* name : {"balanceDetail", "incomeDetail", "expensesDetail", "savedDetail", "averageDetail"}) {
            QLabel* detail = page.findChild<QLabel*>(name);
            QVERIFY2(detail->height() == detail->sizeHint().height(), name);
        }
    }

    // In a narrow window, or in a language with longer texts, the tiles' texts wrap and the page
    // doesn't scroll sideways
    void longTileTextsWrapInsteadOfScrollingSideways() {
        DashboardPage page;
        page.setUserId(1);
        show(page, pageWidthForOneLineTiles(page) - 100);
        QCOMPARE(page.findChild<QScrollArea*>()->horizontalScrollBar()->maximum(), 0);
        int wrapped = 0;
        for (const char* name : {"balanceDetail", "incomeDetail", "expensesDetail", "savedDetail", "averageDetail"}) {
            QLabel* detail = page.findChild<QLabel*>(name);
            QVERIFY2(detail->height() >= detail->heightForWidth(detail->width()), name);      // Nothing cut off
            if (detail->height() > detail->sizeHint().height()) ++wrapped;
        }
        QVERIFY(wrapped > 0);
        // Still as wide as each other: the short texts don't wrap so that the long ones needn't
        const QList<QFrame*> all = tiles(page);
        for (QFrame* tile : all) {
            QVERIFY(qAbs(tile->width() - all.first()->width()) <= 1);
        }
    }

    // Just room for every text on one line: nothing wraps, though the tiles then differ a little
    void withJustEnoughRoomNothingWraps() {
        DashboardPage page;
        page.setUserId(1);
        show(page, pageWidthForOneLineTiles(page));
        for (const char* name : {"balanceDetail", "incomeDetail", "expensesDetail", "savedDetail", "averageDetail"}) {
            QLabel* detail = page.findChild<QLabel*>(name);
            QVERIFY2(detail->height() == detail->sizeHint().height(), name);
        }
        // One pixel less, and they share the row equally
        page.resize(page.width() - 1, page.height());
        QTest::qWait(50);
        const QList<QFrame*> all = tiles(page);
        for (QFrame* tile : all) {
            QVERIFY(qAbs(tile->width() - all.first()->width()) <= 1);
        }
    }

    // A tile with its text on one line keeps it at the top, in line with a higher tile beside it
    void tileTextsLineUpAtTheTop() {
        DashboardPage page;
        page.setUserId(1);
        show(page, pageWidthForOneLineTiles(page) - 100);
        QList<int> captions;
        QList<int> values;
        for (QFrame* tile : tiles(page)) {
            captions << tile->findChild<QLabel*>("tileCaption")->y();
            for (QLabel* label : tile->findChildren<QLabel*>()) {
                if (label->objectName().endsWith("Value")) values << label->y();
            }
        }
        QCOMPARE(values.size(), 5);
        for (int i = 1; i < 5; ++i) {
            QCOMPARE(captions[i], captions[0]);
            QCOMPARE(values[i], values[0]);
        }
    }

    // A large balance makes the balance tile the one that needs most at its narrowest; the others
    // are as wide, so it isn't cut off
    void theTileThatNeedsMostSetsTheWidth() {
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "ACME", 123456789000, today, "", 0, "income")) > 0);
        DashboardPage page;
        page.setUserId(1);
        show(page, 300);
        QFrame* balance = page.findChild<QFrame*>("balanceTile");
        for (QFrame* tile : tiles(page)) {
            QVERIFY(balance->minimumSizeHint().width() >= tile->minimumSizeHint().width());
            QVERIFY2(tile->width() >= balance->minimumSizeHint().width(), qPrintable(tile->objectName()));
        }
    }

    // New texts after a reload, without the page changing size: the tiles are arranged anew
    void reloadingArrangesTheTilesAnew() {
        DashboardPage page;
        page.setUserId(1);
        show(page, pageWidthForOneLineTiles(page));         // Just room: every text on one line
        QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giro, "ACME", 123456789000, today, "", 0, "income")) > 0);
        spend(500, rent);
        page.reload();                                      // Longer texts, which no longer fit
        QTest::qWait(50);
        const QList<QFrame*> all = tiles(page);
        for (QFrame* tile : all) {
            QVERIFY2(qAbs(tile->width() - all.first()->width()) <= 1, qPrintable(tile->objectName()));
        }
    }

    // Even at the narrowest, no tile is narrower than its longest word or caption
    void tilesNeverGetTooNarrowForTheirTexts() {
        DashboardPage page;
        page.setUserId(1);
        show(page, 300);
        for (QFrame* tile : tiles(page)) {
            QVERIFY2(tile->width() >= tile->minimumSizeHint().width(), qPrintable(tile->objectName()));
            for (QLabel* label : tile->findChildren<QLabel*>()) {
                QVERIFY2(label->width() >= label->minimumSizeHint().width(), qPrintable(label->objectName()));
            }
        }
    }

    void emptyCardsWrapToo() {
        DashboardPage page;
        page.setUserId(1);
        for (const char* name : {"budgetEmptyLabel", "categoryEmptyLabel", "recentEmptyLabel"}) {
            QVERIFY2(page.findChild<QLabel*>(name)->wordWrap(), name);
        }
        QVERIFY(qobject_cast<WrapLabel*>(page.findChild<QLabel*>("recentEmptyLabel")));
        QVERIFY(qobject_cast<WrapLabel*>(page.findChild<QLabel*>("categoryEmptyLabel")));
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

    void recentExpensesCanBeRed() {
        spend(1000, groceries);
        Settings::instance().setExpensesInRed(false);
        DashboardPage page;
        page.setUserId(1);
        QAbstractButton* recent = page.findChildren<QAbstractButton*>("recentRow").value(0);
        QVERIFY(recent);
        const QImage plain = recent->grab().toImage();
        auto* chart = page.findChild<QWidget*>("cashFlowChart");
        chart->resize(600, 240);
        const QImage plainChart = chart->grab().toImage();
        auto* tile = page.findChild<QLabel*>("expensesValue");
        QCOMPARE(tile->palette().color(QPalette::WindowText), page.palette().color(QPalette::WindowText));
        Settings::instance().setExpensesInRed(true);
        QVERIFY(recent->grab().toImage() != plain);
        QVERIFY(chart->grab().toImage() != plainChart);  // The expense bars and their legend
        page.applySettings();                            // The tile follows once the settings are applied
        QCOMPARE(tile->palette().color(QPalette::WindowText), Theme::colors().danger);
        Settings::instance().setExpensesInRed(false);
        page.applySettings();
        QCOMPARE(tile->palette().color(QPalette::WindowText), page.palette().color(QPalette::WindowText));
    }

    void withoutAUserThereIsNothingToShow() {
        DashboardPage page;
        page.reload();
        QVERIFY(page.findChildren<QAbstractButton*>("categoryRow").isEmpty());
        page.setPalette(QPalette(Qt::darkBlue));         // A new theme fills no tiles either
        for (QLabel* label : page.findChildren<QLabel*>()) QVERIFY(label->text() != "No change this month");
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

    void cardTitlesSideBySideLineUp() {
        DashboardPage page;
        page.setUserId(1);
        page.resize(1200, 900);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        // The budget card has a link beside its title, the spending card none; their titles are as high
        QList<int> tops;
        for (QLabel* title : page.findChildren<QLabel*>("cardTitle")) {
            if (title->text().startsWith("Budget") || title->text().startsWith("Spending")) {
                tops.append(title->mapTo(title->parentWidget(), QPoint(0, 0)).y() + title->height() / 2);
            }
        }
        QCOMPARE(tops.size(), 2);
        QCOMPARE(tops[0], tops[1]);
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
