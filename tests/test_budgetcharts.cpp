#include <QtTest>
#include <QMouseEvent>

#include "categorycolors.h"
#include "settings.h"
#include "theme.h"
#include "widgets/budgetcharts.h"

namespace {

const ReportRange November{QDate(2026, 11, 1), QDate(2026, 11, 30)};

// Spending added up after each day from the 1st up to the 18th
QList<Cents> byDay(const QList<std::pair<int, Cents>>& spending, int days = 18) {
    QList<Cents> result(days, 0);
    for (const auto& [day, cents] : spending) result[day - 1] += cents;
    for (int day = 1; day < days; ++day) result[day] += result[day - 1];
    return result;
}

// Eating out over its limit, Groceries and Fuel under, Rent paid on the 1st
QList<BudgetLinesChart::Line> fourLines() {
    return {{1, "Eating out", Qt::red, 15000, byDay({{3, 2400}, {6, 850}, {9, 4200}, {12, 650}, {13, 900}, {15, 3800}, {16, 700}, {17, 2100}, {18, 1400}})},
            {2, "Groceries", Qt::darkGreen, 40000, byDay({{1, 6420}, {4, 3890}, {7, 7140}, {9, 2210}, {11, 4130}, {14, 5360}, {17, 2050}})},
            {3, "Fuel", Qt::blue, 12000, byDay({{5, 3100}, {15, 2700}, {16, -700}})},
            {4, "Rent", Qt::gray, 90000, byDay({{1, 90000}})}};
}

ReportRange monthOf(int year, int month) {
    const QDate first(year, month, 1);
    return {first, first.addMonths(1).addDays(-1)};
}

QList<ReportRange> junToNov() {
    QList<ReportRange> months;
    for (int month = 6; month <= 11; ++month) months.append(monthOf(2026, month));
    return months;
}

// Shares in percent per month from June; -1 = no budget that month
BudgetActual budgetOver(int id, const QString& name, Cents limit, const QList<int>& percents) {
    BudgetActual budget;
    budget.categoryId = id;
    budget.categoryName = name;
    for (int i = 0; i < percents.size(); ++i) {
        if (percents[i] < 0) continue;
        budget.byMonth.append({monthOf(2026, 6 + i), limit, limit * percents[i] / 100});
    }
    return budget;
}

QList<BudgetActual> fourBudgetsOverMonths() {
    return {budgetOver(1, "Eating out", 15000, {92, 108, 85, 121, 97, 113}),
            budgetOver(2, "Groceries", 40000, {88, 94, 101, 79, 92, 78}),
            budgetOver(3, "Fuel", 12000, {60, 75, -1, 90, 52, 46}),
            budgetOver(4, "Rent", 90000, {-1, -1, -1, 100, 100, 100})};
}

QList<BudgetMonthsChart::Line> monthLines(const QList<BudgetActual>& budgets) {
    const QList<QColor> colors = {Qt::red, Qt::darkGreen, Qt::blue, Qt::gray};
    QList<BudgetMonthsChart::Line> lines;
    for (int i = 0; i < budgets.size(); ++i) lines.append({budgets[i].categoryId, budgets[i].categoryName, colors[i], budgets[i].byMonth});
    return lines;
}

void moveMouse(QWidget* widget, const QPointF& position) {
    QMouseEvent move(QEvent::MouseMove, position, widget->mapToGlobal(position), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &move);
}

QString money(Cents cents) { return Settings::instance().formatCurrency(cents); }
QString spending(Cents cents) { return Settings::instance().formatSpending(cents); }

double hueApart(const QColor& a, const QColor& b) {
    const double apart = std::abs(a.hslHueF() - b.hslHueF());
    return qMin(apart, 1.0 - apart);
}

} // namespace

class TestBudgetCharts : public QObject {
    Q_OBJECT

private slots:
    void lineColorsKeepApart() {
        const bool shown = Settings::instance().isCategoryColorsInOverviewsEnabled();
        Settings::instance().setCategoryColorsInOverviewsEnabled(true);
        const QList<QColor> own = budgetLineColors({"teal", "", "", "red"});
        QCOMPARE(own[0], CategoryColors::swatch("teal").solid);   // Their own colours
        QCOMPARE(own[3], CategoryColors::swatch("red").solid);
        QCOMPARE(QSet<QRgb>({own[0].rgb(), own[1].rgb(), own[2].rgb(), own[3].rgb()}).size(), 4);
        QVERIFY(hueApart(own[1], own[0]) > 0.15);                 // Far from those in use
        QVERIFY(hueApart(own[1], own[3]) > 0.15);

        // Without category colours in overviews, palette colours for all; grey and slate only
        // when nothing livelier is left (16 budgets, 18 colours)
        Settings::instance().setCategoryColorsInOverviewsEnabled(false);
        const QList<QColor> palette = budgetLineColors(QStringList(16, "teal"));
        QSet<QRgb> distinct;
        for (const QColor& color : palette) {
            distinct.insert(color.rgb());
            for (const char* muted : {"gray", "slate"}) QVERIFY(color != CategoryColors::swatch(muted).solid);
        }
        QCOMPARE(distinct.size(), 16);
        QCOMPARE(budgetLineColors(QStringList(20, "")).size(), 20);   // More than the palette: they repeat
        Settings::instance().setCategoryColorsInOverviewsEnabled(shown);
    }

    void linesShowEachBudgetAgainstItsLimit() {
        BudgetLinesChart chart;
        chart.resize(760, 220);
        chart.setLines(November, fourLines());
        chart.show();                                    // Mouse moves reach visible widgets only
        QVERIFY(QTest::qWaitForWindowExposed(&chart));
        QCOMPARE(chart.lastDay(), 18);

        // Shares of each limit: Rent full from the 1st, Eating out above it, Groceries below
        QCOMPARE(chart.pointOn(3, 1).y(), chart.pointOn(3, 18).y());
        QVERIFY(chart.pointOn(0, 18).y() < chart.pointOn(3, 18).y());
        QVERIFY(chart.pointOn(1, 18).y() > chart.pointOn(3, 18).y());
        QCOMPARE(chart.pointOn(1, 0).y(), chart.pointOn(3, 0).y());       // Everyone starts at nothing
        QVERIFY(chart.pointOn(1, 0).y() > chart.pointOn(1, 1).y());
        QVERIFY(chart.pointOn(1, 18).x() > chart.pointOn(1, 10).x());
        QVERIFY(chart.rect().contains(chart.pointOn(0, 18).toPoint()));

        // Hovering a line names it and that day
        const QPointF groceriesDay10 = chart.pointOn(1, 10);
        QCOMPARE(chart.lineAt(groceriesDay10), 1);
        QCOMPARE(chart.hoverText(groceriesDay10), QString("Groceries, 10 Nov: %1 of %2 (49%)").arg(spending(19660), money(40000)));
        const QPointF rentStart = chart.pointOn(3, 0) + 0.2 * (chart.pointOn(3, 1) - chart.pointOn(3, 0));
        QCOMPARE(chart.hoverText(rentStart), QString("Rent, 1 Nov: %1 of %2 (100%)").arg(spending(90000), money(90000)));
        const QPointF empty(chart.width() - 4, chart.height() / 2.0);
        QCOMPARE(chart.lineAt(empty), -1);
        QCOMPARE(chart.hoverText(empty), QString());

        const QImage plain = chart.grab().toImage();
        moveMouse(&chart, groceriesDay10);
        QCOMPARE(chart.cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(chart.grab().toImage() != plain);
        const QPoint onRent = chart.pointOn(3, 10).toPoint();               // The others fade
        QVERIFY(chart.grab().toImage().pixelColor(onRent) != plain.pixelColor(onRent));
        moveMouse(&chart, empty);
        QCOMPARE(chart.cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(chart.grab().toImage(), plain);

        // Clicking a line asks for its budget
        QSignalSpy clicked(&chart, &BudgetLinesChart::budgetClicked);
        QTest::mouseClick(&chart, Qt::RightButton, {}, groceriesDay10.toPoint());
        QTest::mouseClick(&chart, Qt::LeftButton, {}, empty.toPoint());
        QCOMPARE(clicked.size(), 0);
        QTest::mouseClick(&chart, Qt::LeftButton, {}, groceriesDay10.toPoint());
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(clicked.first().first().toInt(), 2);
    }

    void chartsLetGoWhenTheMouseLeaves() {
        BudgetLinesChart lines;
        lines.resize(760, 220);
        lines.setLines(November, fourLines());
        BudgetMonthsChart months;
        months.resize(760, 220);
        months.setLines(junToNov(), monthLines(fourBudgetsOverMonths()));
        const QList<std::pair<QWidget*, QPointF>> charts = {{&lines, lines.pointOn(1, 10)}, {&months, *months.pointOn(0, 3)}};
        for (const auto& [chart, onLine] : charts) {
            chart->show();
            QVERIFY(QTest::qWaitForWindowExposed(chart));
            const QImage plain = chart->grab().toImage();
            moveMouse(chart, onLine);
            QVERIFY(chart->grab().toImage() != plain);
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(chart, &leave);
            QCOMPARE(chart->grab().toImage(), plain);
        }
    }

    void linesMakeRoomAndStopAtTheLastDay() {
        // Far over the limit, and more back than spent
        BudgetLinesChart chart;
        chart.resize(200, 200);
        chart.setLines(November, {{1, "Over", Qt::red, 1000, byDay({{2, 3000}})}, {2, "Back", Qt::blue, 1000, byDay({{2, -500}})}});
        for (int line : {0, 1}) QVERIFY(chart.rect().contains(chart.pointOn(line, 18).toPoint()));
        QVERIFY(chart.pointOn(1, 18).y() > chart.pointOn(1, 0).y());       // Below the start
        // Just past the last point, still the last day
        QVERIFY(chart.hoverText(chart.pointOn(0, 18) + QPointF(5, 0)).startsWith("Over, 18 Nov: "));
    }

    void monthsShowEachBudgetMonthByMonth() {
        BudgetMonthsChart chart;
        chart.resize(760, 220);
        chart.setLines(junToNov(), monthLines(fourBudgetsOverMonths()));
        chart.show();
        QVERIFY(QTest::qWaitForWindowExposed(&chart));

        QVERIFY(!chart.pointOn(2, 2));                                      // Fuel had no budget in August
        QVERIFY(!chart.pointOn(3, 0));
        QVERIFY(chart.pointOn(3, 3));
        QCOMPARE(chart.pointOn(3, 3)->y(), chart.pointOn(3, 5)->y());       // Rent always 100%
        QVERIFY(chart.pointOn(0, 3)->y() < chart.pointOn(3, 3)->y());       // Eating out over in September
        QVERIFY(chart.pointOn(1, 3)->y() > chart.pointOn(3, 3)->y());
        QVERIFY(chart.pointOn(0, 1)->x() > chart.pointOn(0, 0)->x());
        QVERIFY(chart.rect().contains(chart.pointOn(0, 3)->toPoint()));

        const QPointF september = *chart.pointOn(0, 3);
        QCOMPARE(chart.pointAt(september), std::pair(0, 3));
        QCOMPARE(chart.hoverText(september), QString("Eating out, Sep 2026: %1 of %2 (121%)").arg(spending(18150), money(15000)));
        const QPointF empty(chart.width() - 2, 2);
        QCOMPARE(chart.pointAt(empty), std::pair(-1, -1));
        QCOMPARE(chart.hoverText(empty), QString());

        const QImage plain = chart.grab().toImage();
        moveMouse(&chart, september);
        QCOMPARE(chart.cursor().shape(), Qt::PointingHandCursor);
        const QPoint onGroceries = chart.pointOn(1, 0)->toPoint();          // The others fade
        QVERIFY(chart.grab().toImage().pixelColor(onGroceries) != plain.pixelColor(onGroceries));
        moveMouse(&chart, empty);
        QCOMPARE(chart.cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(chart.grab().toImage(), plain);

        QSignalSpy clicked(&chart, &BudgetMonthsChart::monthClicked);
        QTest::mouseClick(&chart, Qt::RightButton, {}, september.toPoint());
        QTest::mouseClick(&chart, Qt::LeftButton, {}, empty.toPoint());
        QCOMPARE(clicked.size(), 0);
        QTest::mouseClick(&chart, Qt::LeftButton, {}, september.toPoint());
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(clicked.first()[0].toInt(), 1);
        QCOMPARE(clicked.first()[1].value<ReportRange>(), monthOf(2026, 9));

        // Far over still fits
        BudgetMonthsChart over;
        over.resize(400, 200);
        over.setLines(junToNov(), monthLines({budgetOver(1, "Over", 1000, {300, 50, 50, 50, 50, 50})}));
        QVERIFY(over.rect().contains(over.pointOn(0, 0)->toPoint()));
    }

    void theGridShowsEachMonthsShare() {
        BudgetGrid grid;
        grid.resize(760, 200);
        grid.setBudgets(junToNov(), fourBudgetsOverMonths());
        grid.show();
        QVERIFY(QTest::qWaitForWindowExposed(&grid));
        QCOMPARE(grid.sizeHint().height(), 30 * 5 + 2);                    // A heading and four budgets
        QVERIFY(grid.minimumHeight() >= grid.sizeHint().height());
        QCOMPARE(grid.cellText(0, 3), QString("121%"));
        QCOMPARE(grid.cellText(2, 2), QString("–"));                       // Fuel had no budget in August
        QCOMPARE(grid.cellAt(grid.cellRect(1, 4).center()), std::pair(1, 4));
        QCOMPARE(grid.cellAt(QPoint(2, 2)), std::pair(-1, -1));
        QVERIFY(grid.cellRect(0, 1).left() > grid.cellRect(0, 0).right());
        QVERIFY(grid.cellRect(1, 0).top() > grid.cellRect(0, 0).bottom());

        // Red over the limit, amber from 90%, plain below, nothing without a budget
        const QImage image = grid.grab().toImage();
        const auto background = [&](int row, int month) {
            const QRect cell = grid.cellRect(row, month);
            return image.pixelColor(cell.left() + 4, cell.center().y());
        };
        QCOMPARE(background(0, 3), CategoryColors::swatch("red").background);
        QCOMPARE(background(0, 0), CategoryColors::swatch("amber").background);   // 92%
        const QColor plain = background(2, 0);                                   // 60%: the translucent track
        QVERIFY(plain != CategoryColors::swatch("amber").background && plain != CategoryColors::swatch("red").background);
        QVERIFY(background(2, 2) != plain);

        moveMouse(&grid, grid.cellRect(0, 3).center());
        QCOMPARE(grid.cursor().shape(), Qt::PointingHandCursor);
        moveMouse(&grid, grid.cellRect(2, 2).center());
        QCOMPARE(grid.cursor().shape(), Qt::ArrowCursor);

        QSignalSpy clicked(&grid, &BudgetGrid::monthClicked);
        QTest::mouseClick(&grid, Qt::LeftButton, {}, grid.cellRect(2, 2).center());      // Nothing to show
        QTest::mouseClick(&grid, Qt::RightButton, {}, grid.cellRect(0, 3).center());
        QCOMPARE(clicked.size(), 0);
        QTest::mouseClick(&grid, Qt::LeftButton, {}, grid.cellRect(0, 3).center());
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(clicked.first()[0].toInt(), 1);
        QCOMPARE(clicked.first()[1].value<ReportRange>(), monthOf(2026, 9));
    }

};

QTEST_MAIN(TestBudgetCharts)
#include "test_budgetcharts.moc"
