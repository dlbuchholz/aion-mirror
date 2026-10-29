#include "report.h"

#include <QLocale>

namespace {

const QLocale english(QLocale::English);

const QList<QPair<ReportRange::Preset, QString>> PresetKeys = {
    {ReportRange::Preset::Month, "month"},
    {ReportRange::Preset::ThreeMonths, "3months"},
    {ReportRange::Preset::SixMonths, "6months"},
    {ReportRange::Preset::Year, "year"},
    {ReportRange::Preset::Custom, "custom"},
};

QDate monthStart(const QDate& date) {
    return QDate(date.year(), date.month(), 1);
}

QDate monthEnd(const QDate& date) {
    return monthStart(date).addMonths(1).addDays(-1);
}

} // namespace

ReportRange ReportRange::forPreset(Preset preset, const QDate& today) {
    int months = 1;
    switch (preset) {
    case Preset::Month: case Preset::Custom: months = 1; break;
    case Preset::ThreeMonths: months = 3; break;
    case Preset::SixMonths: months = 6; break;
    case Preset::Year: months = 12; break;
    }
    return {monthStart(today).addMonths(1 - months), monthEnd(today)};
}

ReportRange ReportRange::custom(QDate from, QDate to) {
    if (from > to) std::swap(from, to);
    return {from, to};
}

bool ReportRange::isWholeMonths() const {
    return isValid() && first.day() == 1 && last == monthEnd(last);
}

int ReportRange::months() const {
    return isValid() ? (last.year() - first.year()) * 12 + last.month() - first.month() + 1 : 0;
}

ReportRange ReportRange::comparison(const QDate& today) const {
    if (!isValid()) return {};
    const bool ongoing = first <= today && today < last;
    if (isWholeMonths()) {
        const int count = months();
        ReportRange before{first.addMonths(-count), first.addDays(-1)};
        if (ongoing) before.last = qMin(before.last, today.addMonths(-count));   // 31 May less 3 months is 28 Feb
        return before;
    }
    ReportRange before{first.addDays(-days()), first.addDays(-1)};
    if (ongoing) before.last = before.first.addDays(first.daysTo(today));
    return before;
}

QList<ReportRange> ReportRange::periods(Unit unit) const {
    QList<ReportRange> result;
    if (!isValid()) return result;
    QDate start = first;
    while (start <= last) {
        QDate end;
        switch (unit) {
        case Unit::Day: end = start; break;
        case Unit::Week: end = start.addDays(7 - start.dayOfWeek()); break;   // Up to Sunday
        case Unit::Month: end = monthEnd(start); break;
        }
        end = qMin(end, last);
        result.append({start, end});
        start = end.addDays(1);
    }
    return result;
}

ReportRange::Unit ReportRange::flowUnit() const {
    return days() <= 45 ? Unit::Week : Unit::Month;
}

QString ReportRange::label() const {
    if (!isValid()) return QString();
    const bool sameYear = first.year() == last.year();
    if (isWholeMonths()) {
        if (months() == 1) return english.toString(first, "MMMM yyyy");
        return sameYear ? QString("%1 – %2").arg(english.toString(first, "MMMM"), english.toString(last, "MMMM yyyy"))
                        : QString("%1 – %2").arg(english.toString(first, "MMMM yyyy"), english.toString(last, "MMMM yyyy"));
    }
    if (first == last) return english.toString(first, "d MMM yyyy");
    return QString("%1 – %2").arg(english.toString(first, sameYear ? "d MMM" : "d MMM yyyy"), english.toString(last, "d MMM yyyy"));
}

QString ReportRange::key(Preset preset) {
    for (const auto& [candidate, name] : PresetKeys) {
        if (candidate == preset) return name;
    }
    return QString();
}

std::optional<ReportRange::Preset> ReportRange::presetFromKey(const QString& key) {
    for (const auto& [preset, name] : PresetKeys) {
        if (name == key) return preset;
    }
    return std::nullopt;
}
