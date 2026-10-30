#ifndef REPORT_H
#define REPORT_H

#include <QDate>
#include <QList>
#include <QString>
#include <optional>
#include "money.h"

// The days a report covers, from first to last
struct ReportRange {
    // The choices on the Reports page. The month ones are whole calendar months, ending with this one.
    enum class Preset { Month, ThreeMonths, SixMonths, Year, Custom };
    // How a report groups the range: days, weeks (Monday to Sunday) or months
    enum class Unit { Day, Week, Month };

    QDate first;
    QDate last;

    static ReportRange forPreset(Preset preset, const QDate& today);   // Custom gives this month
    static ReportRange custom(QDate from, QDate to);                   // Back to front is turned round

    bool isValid() const { return first.isValid() && last.isValid() && first <= last; }
    bool operator==(const ReportRange& other) const { return first == other.first && last == other.last; }
    qint64 days() const { return first.daysTo(last) + 1; }
    bool isWholeMonths() const;                      // From the 1st to the end of a month
    int months() const;                              // Calendar months it touches

    // The range to compare with: the same number of months (or days) directly before. While the
    // range runs past today, only up to the same point, like the Dashboard does: "the 6 months
    // before, up to the 29th".
    ReportRange comparison(const QDate& today) const;

    // The range cut into days, weeks or months; the first and last ones can be partial
    QList<ReportRange> periods(Unit unit) const;
    // Weeks for up to about a month and a half, months for longer
    Unit flowUnit() const;

    // "October 2026", "May – October 2026", "1 Sep – 15 Oct 2026"
    QString label() const;
    // The calendar week (ISO, Monday to Sunday) the range starts in: "Week 40"
    QString weekName() const;
    // What comparison() covers, in words: "the 6 months before", "the month before, up to 29 September"
    QString comparisonText(const QDate& today) const;

    // For the settings: "month", "3months", "6months", "year", "custom"
    static QString key(Preset preset);
    static std::optional<Preset> presetFromKey(const QString& key);
};

// One category's spending in a range and in the range before it. Refunds lower it, so it can be
// below zero.
struct CategorySpent {
    int categoryId = 0;                              // 0 = no category
    QString categoryName;
    QString categoryColor;                           // Palette name, empty = neutral
    Cents spentCents = 0;
    Cents beforeCents = 0;
};

struct SpendingReport {
    ReportRange range;
    ReportRange before;                              // range.comparison()
    Cents totalCents = 0;
    Cents beforeTotalCents = 0;
    QList<CategorySpent> categories;                 // Those with spending in the range, largest first
};

// Income and spending in one week or month of a report
struct PeriodFlow {
    ReportRange period;
    Cents incomeCents = 0;
    Cents spendingCents = 0;                         // Less refunds
    Cents netCents() const { return incomeCents - spendingCents; }
};

struct FlowReport {
    ReportRange range;
    QList<PeriodFlow> periods;                       // In order, weeks or months by range.flowUnit()
    Cents incomeCents = 0;
    Cents spendingCents = 0;
    Cents netCents() const { return incomeCents - spendingCents; }
    // How much of the income was kept, rounded; none without income or when more went out
    std::optional<int> keptPercent() const;
};

struct BalancePoint {
    QDate date;
    Cents balanceCents = 0;                          // At the end of the day
};

struct BalanceReport {
    ReportRange range;
    Cents startCents = 0;                            // At the end of the day before the range
    // Every day from the first to today, or to the last day if the range is over (or all in the future)
    QList<BalancePoint> days;
    Cents endCents() const { return days.isEmpty() ? startCents : days.last().balanceCents; }
    Cents changeCents() const { return endCents() - startCents; }
    // The balance at the end of the given day, or the latest one before it
    Cents balanceOn(const QDate& date) const;
};

// One category's spending in a week or month of a report
struct PeriodSpent {
    ReportRange period;
    Cents spentCents = 0;                            // Less refunds
    int count = 0;                                   // Transactions
};

struct CategoryTrend {
    ReportRange range;
    int categoryId = 0;                              // 0 = no category
    QString categoryName;
    QString categoryColor;
    QList<PeriodSpent> periods;                      // In order, weeks or months by range.flowUnit()
    Cents totalCents = 0;
    int count = 0;
    Cents averageCents = 0;                          // Per week or month, over those that have begun
};

// What went to one payee. Spellings that differ only in case or spaces are one payee.
struct PayeeSpent {
    QString payee;                                   // The most used spelling; empty = no payee
    Cents spentCents = 0;                            // Less refunds
    int count = 0;
};

struct PayeeReport {
    ReportRange range;
    QList<PayeeSpent> payees;                        // All with spending, largest first
    Cents totalCents = 0;
};

namespace Reports {

// Spending per payee, without transfers and balance adjustments. accountId < 0 means all accounts.
PayeeReport topPayees(int userId, int accountId, const ReportRange& range);

// How one category's spending went, week by week or month by month. Without transfers and
// balance adjustments; accountId < 0 means all accounts.
CategoryTrend categoryTrend(int userId, int accountId, int categoryId, const ReportRange& range, const QDate& today);

// The balance day by day, with everything that moves it: transfers and balance adjustments too.
// accountId < 0 means all accounts together.
BalanceReport balanceOverTime(int userId, int accountId, const ReportRange& range, const QDate& today);

// Income and spending per week or month, without transfers and balance adjustments.
// accountId < 0 means all accounts.
FlowReport incomeVsSpending(int userId, int accountId, const ReportRange& range);

// Spending per category, without transfers and balance adjustments. accountId < 0 means all accounts.
SpendingReport spendingByCategory(int userId, int accountId, const ReportRange& range, const QDate& today);

// Share of the income that was kept, rounded; none without income or when more went out than came in
std::optional<int> keptPercent(Cents income, Cents net);

// Rounded change in percent from before to now; none when there was nothing before to compare with
std::optional<int> changePercent(Cents now, Cents before);

} // namespace Reports

#endif // REPORT_H
