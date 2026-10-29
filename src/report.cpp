#include "report.h"
#include "category.h"
#include "db/databasemanager.h"

#include <QHash>
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

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

QString ReportRange::weekName() const {
    return first.isValid() ? QString("Week %1").arg(first.weekNumber()) : QString();
}

QString ReportRange::comparisonText(const QDate& today) const {
    if (!isValid()) return QString();
    QString text;
    if (isWholeMonths()) {
        text = months() == 1 ? QString("the month before") : QString("the %1 months before").arg(months());
    } else {
        text = days() == 1 ? QString("the day before") : QString("the %1 days before").arg(days());
    }
    const bool ongoing = first <= today && today < last;
    if (ongoing) text += ", up to " + english.toString(comparison(today).last, "d MMMM");
    return text;
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

std::optional<int> FlowReport::keptPercent() const {
    return Reports::keptPercent(incomeCents, netCents());
}

Cents BalanceReport::balanceOn(const QDate& date) const {
    Cents balance = startCents;
    for (const BalancePoint& point : days) {
        if (point.date > date) break;
        balance = point.balanceCents;
    }
    return balance;
}

namespace Reports {

BalanceReport balanceOverTime(int userId, int accountId, const ReportRange& range, const QDate& today) {
    BalanceReport report;
    report.range = range;
    if (!range.isValid()) return report;
    const QDate lastShown = range.first <= today ? qMin(range.last, today) : range.last;

    // Everything before the range, then each day's change
    const QString signedAmount = "CASE WHEN t.type = 'income' THEN t.amount WHEN t.type = 'expense' THEN -t.amount ELSE 0 END";
    const QString accounts = accountId >= 0 ? " AND t.account_id = :account_id " : " ";
    QSqlQuery startQuery(DatabaseManager::instance().database());
    startQuery.prepare("SELECT COALESCE(SUM(" + signedAmount + "), 0) FROM transactions t JOIN accounts a ON a.id = t.account_id "
                       "WHERE a.user_id = :user_id AND t.date < :first" + accounts);
    startQuery.bindValue(":user_id", userId);
    startQuery.bindValue(":first", range.first.toString("yyyy-MM-dd"));
    if (accountId >= 0) startQuery.bindValue(":account_id", accountId);
    if (!startQuery.exec() || !startQuery.next()) {
        qWarning() << "Reports:" << startQuery.lastError().text();
        return report;
    }
    report.startCents = startQuery.value(0).toLongLong();

    QSqlQuery dayQuery(DatabaseManager::instance().database());
    dayQuery.prepare("SELECT t.date, SUM(" + signedAmount + ") FROM transactions t JOIN accounts a ON a.id = t.account_id "
                     "WHERE a.user_id = :user_id AND t.date BETWEEN :first AND :last" + accounts + "GROUP BY t.date");
    dayQuery.bindValue(":user_id", userId);
    dayQuery.bindValue(":first", range.first.toString("yyyy-MM-dd"));
    dayQuery.bindValue(":last", lastShown.toString("yyyy-MM-dd"));
    if (accountId >= 0) dayQuery.bindValue(":account_id", accountId);
    if (!dayQuery.exec()) {
        qWarning() << "Reports:" << dayQuery.lastError().text();
        return report;
    }
    QHash<QDate, Cents> changes;
    while (dayQuery.next()) {
        changes.insert(QDate::fromString(dayQuery.value(0).toString(), "yyyy-MM-dd"), dayQuery.value(1).toLongLong());
    }
    Cents balance = report.startCents;
    for (QDate day = range.first; day <= lastShown; day = day.addDays(1)) {
        balance += changes.value(day);
        report.days.append({day, balance});
    }
    return report;
}

SpendingReport spendingByCategory(int userId, int accountId, const ReportRange& range, const QDate& today) {
    SpendingReport report;
    report.range = range;
    report.before = range.comparison(today);
    if (!range.isValid()) return report;

    // Both ranges in one go; the one before ends where this one starts
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "SELECT COALESCE(t.category_id, 0), c.name, c.color, "
        "SUM(CASE WHEN t.date >= :first THEN " + SpendingCentsSql + " ELSE 0 END), "
        "SUM(CASE WHEN t.date BETWEEN :beforeFirst AND :beforeLast THEN " + SpendingCentsSql + " ELSE 0 END) "
        "FROM transactions t JOIN accounts a ON a.id = t.account_id LEFT JOIN categories c ON c.id = t.category_id "
        "WHERE a.user_id = :user_id AND " + IsSpendingSql + " AND t.date BETWEEN :from AND :last AND "
        + CountedAsIncomeOrSpendingSql + (accountId >= 0 ? " AND t.account_id = :account_id " : " ")
        + "GROUP BY COALESCE(t.category_id, 0)");
    const ReportRange& before = report.before;
    query.bindValue(":first", range.first.toString("yyyy-MM-dd"));
    query.bindValue(":beforeFirst", before.first.toString("yyyy-MM-dd"));
    query.bindValue(":beforeLast", before.last.toString("yyyy-MM-dd"));
    query.bindValue(":user_id", userId);
    query.bindValue(":from", before.first.toString("yyyy-MM-dd"));
    query.bindValue(":last", range.last.toString("yyyy-MM-dd"));
    if (accountId >= 0) query.bindValue(":account_id", accountId);
    if (!query.exec()) {
        qWarning() << "Reports:" << query.lastError().text();
        return report;
    }
    while (query.next()) {
        CategorySpent spent;
        spent.categoryId = query.value(0).toInt();
        spent.categoryName = spent.categoryId == 0 ? QString("No category") : query.value(1).toString();
        spent.categoryColor = query.value(2).toString();
        spent.spentCents = query.value(3).toLongLong();
        spent.beforeCents = query.value(4).toLongLong();
        report.totalCents += spent.spentCents;
        report.beforeTotalCents += spent.beforeCents;
        if (spent.spentCents != 0) report.categories.append(spent);
    }
    std::sort(report.categories.begin(), report.categories.end(), [](const CategorySpent& a, const CategorySpent& b) {
        return a.spentCents != b.spentCents ? a.spentCents > b.spentCents
                                            : a.categoryName.localeAwareCompare(b.categoryName) < 0;
    });
    return report;
}

FlowReport incomeVsSpending(int userId, int accountId, const ReportRange& range) {
    FlowReport report;
    report.range = range;
    if (!range.isValid()) return report;
    for (const ReportRange& period : range.periods(range.flowUnit())) {
        PeriodFlow flow;
        flow.period = period;
        report.periods.append(flow);
    }

    // Per day, then into the periods; refunds lower the spending instead of counting as income
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare(
        "SELECT t.date, "
        "SUM(CASE WHEN " + IsIncomeSql + " THEN t.amount ELSE 0 END), "
        "SUM(CASE WHEN " + IsSpendingSql + " THEN " + SpendingCentsSql + " ELSE 0 END) "
        "FROM transactions t JOIN accounts a ON a.id = t.account_id LEFT JOIN categories c ON c.id = t.category_id "
        "WHERE a.user_id = :user_id AND t.date BETWEEN :first AND :last AND " + CountedAsIncomeOrSpendingSql
        + (accountId >= 0 ? " AND t.account_id = :account_id " : " ") + "GROUP BY t.date ORDER BY t.date");
    query.bindValue(":user_id", userId);
    query.bindValue(":first", range.first.toString("yyyy-MM-dd"));
    query.bindValue(":last", range.last.toString("yyyy-MM-dd"));
    if (accountId >= 0) query.bindValue(":account_id", accountId);
    if (!query.exec()) {
        qWarning() << "Reports:" << query.lastError().text();
        return report;
    }
    int index = 0;
    while (query.next()) {
        const QDate date = QDate::fromString(query.value(0).toString(), "yyyy-MM-dd");
        while (index < report.periods.size() && report.periods[index].period.last < date) ++index;
        if (index >= report.periods.size()) break;
        PeriodFlow& flow = report.periods[index];
        flow.incomeCents += query.value(1).toLongLong();
        flow.spendingCents += query.value(2).toLongLong();
        report.incomeCents += query.value(1).toLongLong();
        report.spendingCents += query.value(2).toLongLong();
    }
    return report;
}

std::optional<int> keptPercent(Cents income, Cents net) {
    if (income <= 0 || net < 0) return std::nullopt;
    return qRound(double(net) / double(income) * 100.0);
}

std::optional<int> changePercent(Cents now, Cents before) {
    if (before <= 0) return std::nullopt;
    return qRound(double(now - before) / double(before) * 100.0);
}

} // namespace Reports
