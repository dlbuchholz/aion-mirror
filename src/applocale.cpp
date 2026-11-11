#include "applocale.h"
#include "language.h"

namespace {

QString languageOrCurrent(const QString& language) {
    return language.isEmpty() ? Language::current() : language;
}

// The pattern of a format in a language; English for languages without their own
QString pattern(AppLocale::DateFormat format, const QString& language) {
    using F = AppLocale::DateFormat;
    if (language == "de") {
        switch (format) {
        case F::DayMonthYear:          return "d. MMMM yyyy";
        case F::DayMonth:              return "d. MMMM";
        case F::MonthYear:             return "MMMM yyyy";
        case F::Month:                 return "MMMM";
        case F::ShortDayMonthYear:     return "d. MMM yyyy";
        case F::ShortDayMonth:         return "d. MMM";
        case F::ShortMonthYear:        return "MMM yyyy";
        case F::ShortMonth:            return "MMM";
        case F::WeekdayDayMonth:       return "dddd, d. MMMM";
        case F::WeekdayDayMonthYear:   return "dddd, d. MMMM yyyy";
        case F::ShortDayMonthYearTime: return "d. MMM yyyy, HH:mm";
        }
    }
    switch (format) {
    case F::DayMonthYear:          return "d MMMM yyyy";
    case F::DayMonth:              return "d MMMM";
    case F::MonthYear:             return "MMMM yyyy";
    case F::Month:                 return "MMMM";
    case F::ShortDayMonthYear:     return "d MMM yyyy";
    case F::ShortDayMonth:         return "d MMM";
    case F::ShortMonthYear:        return "MMM yyyy";
    case F::ShortMonth:            return "MMM";
    case F::WeekdayDayMonth:       return "dddd, d MMMM";
    case F::WeekdayDayMonthYear:   return "dddd, d MMMM yyyy";
    case F::ShortDayMonthYearTime: return "d MMM yyyy, HH:mm";
    }
    return QString();
}

} // namespace

namespace AppLocale {

QLocale locale(const QString& language) {
    const QString code = languageOrCurrent(language);
    if (code == "de") return QLocale(QLocale::German, QLocale::Germany);
    return QLocale(QLocale::English);
}

QString date(const QDate& date, DateFormat format, const QString& language) {
    const QString code = languageOrCurrent(language);
    // A month on its own is named as it is on its own, "Nov" rather than German's "Nov."
    if (format == DateFormat::Month || format == DateFormat::ShortMonth) {
        return date.isValid() ? monthName(date.month(), format == DateFormat::Month ? QLocale::LongFormat : QLocale::ShortFormat, code)
                              : QString();
    }
    return locale(code).toString(date, pattern(format, code));
}

QString dateTime(const QDateTime& dateTime, DateFormat format, const QString& language) {
    const QString code = languageOrCurrent(language);
    return locale(code).toString(dateTime, pattern(format, code));
}

QString monthName(int month, QLocale::FormatType type, const QString& language) {
    return locale(language).standaloneMonthName(month, type);
}

QString number(qint64 value, const QString& language) {
    return locale(language).toString(value);
}

QString count(qint64 value, const QString& one, const QString& many, const QString& language) {
    return (value == 1 ? one : many).arg(number(value, language));
}

QString dataSize(qint64 bytes, int precision, const QString& language) {
    return locale(language).formattedDataSize(bytes, precision);
}

} // namespace AppLocale
