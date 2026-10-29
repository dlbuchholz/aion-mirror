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

namespace Reports {

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
