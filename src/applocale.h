#ifndef APPLOCALE_H
#define APPLOCALE_H

#include <QDate>
#include <QDateTime>
#include <QLocale>
#include <QString>

// Dates, month names, counts and file sizes in the language of Aion's texts (Language::current()),
// so "7 November 2026" becomes "7. November 2026" in German. Money follows the currency settings
// instead, and dates typed or read from files the system's regional format.
namespace AppLocale {

// The ways Aion writes a date, each with its own pattern in each language
enum class DateFormat {
    DayMonthYear,           // 7 November 2026
    DayMonth,               // 7 November
    MonthYear,              // November 2026
    Month,                  // November
    ShortDayMonthYear,      // 7 Nov 2026
    ShortDayMonth,          // 7 Nov
    ShortMonthYear,         // Nov 2026
    ShortMonth,             // Nov
    WeekdayDayMonth,        // Saturday, 7 November
    WeekdayDayMonthYear,    // Saturday, 7 November 2026
    ShortDayMonthYearTime,  // 7 Nov 2026, 14:05
};

// The locale of a language, the current one when none is given
QLocale locale(const QString& language = QString());

QString date(const QDate& date, DateFormat format, const QString& language = QString());
QString dateTime(const QDateTime& dateTime, DateFormat format, const QString& language = QString());

// A month's name on its own, January = 1
QString monthName(int month, QLocale::FormatType type = QLocale::LongFormat, const QString& language = QString());

// A whole number with the language's grouping, "1,234" or "1.234"
QString number(qint64 value, const QString& language = QString());

// A count with its noun, one or many: count(3, tr("%1 user"), tr("%1 users")) is "3 users". Each
// form is translated on its own; enough for English and German, which have just these two.
QString count(qint64 value, const QString& one, const QString& many, const QString& language = QString());

// A file size, "1.5 MB" or "1,5 MB"
QString dataSize(qint64 bytes, int precision = 2, const QString& language = QString());

} // namespace AppLocale

#endif // APPLOCALE_H
