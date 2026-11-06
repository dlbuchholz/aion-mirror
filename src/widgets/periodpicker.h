#ifndef PERIODPICKER_H
#define PERIODPICKER_H

#include <QFrame>
#include <QList>
#include <QWidget>
#include <functional>
#include "report.h"

class QLabel;
class QToolButton;
class PeriodMenu;

// The period of the transactions shown: "‹ March 2026 ›". The arrows step to the period before or
// after, of the same length; the name opens a menu with the usual periods and any month. An
// invalid range is all time, which has nothing to step to.
class PeriodPicker : public QFrame {
    Q_OBJECT

public:
    explicit PeriodPicker(QWidget* parent = nullptr);

    std::function<QDate()> today = [] { return QDate::currentDate(); };

    ReportRange period() const { return shown; }
    void setPeriod(const ReportRange& range);        // Without periodChanged

    // The period before (-1) or after (1): whole months by their number of months, others by days
    static ReportRange stepped(const ReportRange& range, int direction);
    // "All time", "2026" for a calendar year, otherwise like the Reports page: "March 2026"
    static QString nameOf(const ReportRange& range);

signals:
    void periodChanged(const ReportRange& range);    // Chosen or stepped to

protected:
    void changeEvent(QEvent* event) override;

private:
    void choose(const ReportRange& range);
    void refresh();
    void updateIcons();
    void updateNameWidth();

    QToolButton* previousButton = nullptr;
    QToolButton* nameButton = nullptr;
    QToolButton* nextButton = nullptr;
    PeriodMenu* menu = nullptr;
    ReportRange shown;
};

// The menu under the name: all time, this month, last month and this year, then a year with
// arrows and its twelve months. Months that haven't started yet can't be chosen.
class PeriodMenu : public QFrame {
    Q_OBJECT

public:
    explicit PeriodMenu(QWidget* parent = nullptr);

    std::function<QDate()> today = [] { return QDate::currentDate(); };

    // Opens below the widget, on the year of the period shown (or this year)
    void showBelow(QWidget* anchor, const ReportRange& current);
    int shownYear() const { return year; }

signals:
    void chosen(const ReportRange& range);

private:
    void showYear(int newYear);
    void choose(const ReportRange& range);

    QToolButton* previousButton = nullptr;
    QToolButton* nextButton = nullptr;
    QLabel* yearLabel = nullptr;
    QFrame* separator = nullptr;                     // Between the usual periods and the months
    QList<QToolButton*> monthButtons;                // January to December
    ReportRange current;
    int year = 0;
};

#endif // PERIODPICKER_H
