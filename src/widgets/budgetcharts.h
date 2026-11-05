#ifndef BUDGETCHARTS_H
#define BUDGETCHARTS_H

#include <QDate>
#include <QWidget>
#include "report.h"

// Line colours for budgets: the categories' own colours when they're shown in overviews (Settings,
// Appearance); budgets without one, or all of them otherwise, get the palette colour furthest in
// hue from those already used, so the lines stay apart. Muted colours come last.
QList<QColor> budgetLineColors(const QStringList& categoryColors);

// "Eating out, Sep 2026: 181,50 € of 150,00 € (121 %)"
QString budgetShareText(const QString& name, const QString& when, Cents spentCents, Cents limitCents);

// Each budget of one month, day by day, as a share of its own limit: 100 % is the limit, the
// dashed line an even pace to it. Hovering a line brings it forward and names it; clicking it
// asks for that budget.
class BudgetLinesChart : public QWidget {
    Q_OBJECT

public:
    struct Line {
        int categoryId = 0;
        QString name;
        QColor color;
        Cents limitCents = 0;
        QList<Cents> spentByDay;                     // After each day from the 1st, as BudgetActual::spentByDay
    };

    explicit BudgetLinesChart(QWidget* parent = nullptr);
    void setLines(const ReportRange& month, const QList<Line>& lines);

    int lastDay() const;                             // Up to where the lines go
    QPointF pointOn(int line, int day) const;        // Day 0 is before the 1st
    int lineAt(const QPointF& position) const;       // -1 for none
    QString hoverText(const QPointF& position) const;

signals:
    void budgetClicked(int categoryId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plot() const;
    double xFor(double day) const;
    double yFor(double percent) const;
    double percentOn(int line, int day) const;
    int dayAt(const QPointF& position) const;

    ReportRange month;
    QList<Line> lines;
    double lowest = 0;
    double highest = 110;
    int hovered = -1;
};

// Each budget month by month, as a share of its limit; a month without that budget leaves a gap.
// Hovering a point brings its budget forward and names the month; clicking it asks for that
// budget's month.
class BudgetMonthsChart : public QWidget {
    Q_OBJECT

public:
    struct Line {
        int categoryId = 0;
        QString name;
        QColor color;
        QList<BudgetMonth> byMonth;                  // As BudgetActual::byMonth
    };

    explicit BudgetMonthsChart(QWidget* parent = nullptr);
    void setLines(const QList<ReportRange>& months, const QList<Line>& lines);

    std::optional<QPointF> pointOn(int line, int month) const;   // None in a month without the budget
    std::pair<int, int> pointAt(const QPointF& position) const;  // Line and month, -1 for none
    QString hoverText(const QPointF& position) const;

signals:
    void monthClicked(int categoryId, const ReportRange& month);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRectF plot() const;
    double xFor(int month) const;
    double yFor(double percent) const;
    const BudgetMonth* budgetIn(int line, int month) const;

    QList<ReportRange> months;
    QList<Line> lines;
    double highest = 110;
    int hovered = -1;
};

// Each budget a row, each month a column, each cell the share of the limit used: red when over,
// amber from 90 %. Hovering a cell names it; clicking one asks for that budget's month.
class BudgetGrid : public QWidget {
    Q_OBJECT

public:
    explicit BudgetGrid(QWidget* parent = nullptr);
    void setBudgets(const QList<ReportRange>& months, const QList<BudgetActual>& budgets);

    QRect cellRect(int row, int month) const;
    std::pair<int, int> cellAt(const QPoint& position) const;   // Row and month, -1 for none
    QString cellText(int row, int month) const;                 // "113 %", "–" without a budget

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void monthClicked(int categoryId, const ReportRange& month);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static constexpr int RowHeight = 30;
    int nameWidth() const;
    const BudgetMonth* budgetIn(int row, int month) const;

    QList<ReportRange> months;
    QList<BudgetActual> budgets;
};

#endif // BUDGETCHARTS_H
