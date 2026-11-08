#include <QtTest>

#include "applocale.h"
#include "language.h"

using F = AppLocale::DateFormat;

class TestAppLocale : public QObject {
    Q_OBJECT

    const QDate date{2026, 9, 5};                  // A Saturday in September, the longest short name
    const QDateTime dateTime{QDate(2026, 9, 5), QTime(14, 5)};

private slots:
    void cleanup() { Language::install("en"); }

    void englishLooksAsBefore() {
        QCOMPARE(AppLocale::date(date, F::DayMonthYear, "en"), QString("5 September 2026"));
        QCOMPARE(AppLocale::date(date, F::DayMonth, "en"), QString("5 September"));
        QCOMPARE(AppLocale::date(date, F::MonthYear, "en"), QString("September 2026"));
        QCOMPARE(AppLocale::date(date, F::Month, "en"), QString("September"));
        QCOMPARE(AppLocale::date(date, F::ShortDayMonthYear, "en"), QString("5 Sep 2026"));
        QCOMPARE(AppLocale::date(date, F::ShortDayMonth, "en"), QString("5 Sep"));
        QCOMPARE(AppLocale::date(date, F::ShortMonthYear, "en"), QString("Sep 2026"));
        QCOMPARE(AppLocale::date(date, F::ShortMonth, "en"), QString("Sep"));
        QCOMPARE(AppLocale::date(date, F::WeekdayDayMonth, "en"), QString("Saturday, 5 September"));
        QCOMPARE(AppLocale::date(date, F::WeekdayDayMonthYear, "en"), QString("Saturday, 5 September 2026"));
        QCOMPARE(AppLocale::dateTime(dateTime, F::ShortDayMonthYearTime, "en"), QString("5 Sep 2026, 14:05"));
        QCOMPARE(AppLocale::monthName(3, QLocale::ShortFormat, "en"), QString("Mar"));
        QCOMPARE(AppLocale::monthName(3, QLocale::LongFormat, "en"), QString("March"));
        QCOMPARE(AppLocale::number(1234567, "en"), QString("1,234,567"));
        QCOMPARE(AppLocale::dataSize(1536 * 1024, 1, "en"), QString("1.5 MiB"));
    }

    void germanHasItsOwnPatterns() {
        QCOMPARE(AppLocale::date(date, F::DayMonthYear, "de"), QString("5. September 2026"));
        QCOMPARE(AppLocale::date(date, F::DayMonth, "de"), QString("5. September"));
        QCOMPARE(AppLocale::date(date, F::MonthYear, "de"), QString("September 2026"));
        QCOMPARE(AppLocale::date(date, F::Month, "de"), QString("September"));
        QCOMPARE(AppLocale::date(date, F::ShortDayMonthYear, "de"), QString("5. Sept. 2026"));
        QCOMPARE(AppLocale::date(date, F::ShortDayMonth, "de"), QString("5. Sept."));
        QCOMPARE(AppLocale::date(date, F::ShortMonthYear, "de"), QString("Sept. 2026"));
        QCOMPARE(AppLocale::date(date, F::WeekdayDayMonth, "de"), QString("Samstag, 5. September"));
        QCOMPARE(AppLocale::date(date, F::WeekdayDayMonthYear, "de"), QString("Samstag, 5. September 2026"));
        QCOMPARE(AppLocale::dateTime(dateTime, F::ShortDayMonthYearTime, "de"), QString("5. Sept. 2026, 14:05"));
        QCOMPARE(AppLocale::number(1234567, "de"), QString("1.234.567"));
        QCOMPARE(AppLocale::dataSize(1536 * 1024, 1, "de"), QString("1,5 MiB"));
    }

    void countsPickTheFormAndGroupTheNumber() {
        QCOMPARE(AppLocale::count(1, "%1 user", "%1 users", "en"), QString("1 user"));
        QCOMPARE(AppLocale::count(0, "%1 user", "%1 users", "en"), QString("0 users"));
        QCOMPARE(AppLocale::count(2, "%1 user", "%1 users", "en"), QString("2 users"));
        QCOMPARE(AppLocale::count(1500, "%1 Umsatz", "%1 Umsätze", "de"), QString("1.500 Umsätze"));
        QCOMPARE(AppLocale::count(-1, "%1 day", "%1 days", "en"), QString("-1 days"));
    }

    void aMonthOnItsOwnHasNoFullStop() {
        // Chart axes and the period picker's buttons: "Sep", not the "Sept." used within a date
        QCOMPARE(AppLocale::date(date, F::ShortMonth, "de"), QString("Sep"));
        QCOMPARE(AppLocale::monthName(3, QLocale::ShortFormat, "de"), QString("Mär"));
        QCOMPARE(AppLocale::monthName(3, QLocale::LongFormat, "de"), QString("März"));
        QCOMPARE(AppLocale::date(QDate(), F::ShortMonth, "de"), QString());
    }

    void followsTheInstalledLanguage() {
        QCOMPARE(AppLocale::date(date, F::DayMonth), QString("5 September"));
#ifndef AION_HAS_TRANSLATIONS
        QSKIP("Built without Qt's LinguistTools");
#endif
        QVERIFY(Language::install("de"));
        QCOMPARE(AppLocale::date(date, F::DayMonth), QString("5. September"));
        QCOMPARE(AppLocale::locale().language(), QLocale::German);
        Language::install("en");
        QCOMPARE(AppLocale::date(date, F::DayMonth), QString("5 September"));
    }
};

QTEST_MAIN(TestAppLocale)
#include "test_applocale.moc"
