#ifndef REPORT_H
#define REPORT_H

#include <QDate>
#include <QList>
#include <QString>
#include <optional>

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

    // For the settings: "month", "3months", "6months", "year", "custom"
    static QString key(Preset preset);
    static std::optional<Preset> presetFromKey(const QString& key);
};

#endif // REPORT_H
