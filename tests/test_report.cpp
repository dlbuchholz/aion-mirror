#include <QtTest>
#include "report.h"

using Preset = ReportRange::Preset;
using Unit = ReportRange::Unit;

class TestReport : public QObject {
    Q_OBJECT
    const QDate today = QDate(2026, 10, 29);

    static ReportRange range(int y1, int m1, int d1, int y2, int m2, int d2) {
        return {QDate(y1, m1, d1), QDate(y2, m2, d2)};
    }

private slots:
    void presetsAreWholeMonthsEndingWithThisOne() {
        QCOMPARE(ReportRange::forPreset(Preset::Month, today), range(2026, 10, 1, 2026, 10, 31));
        QCOMPARE(ReportRange::forPreset(Preset::ThreeMonths, today), range(2026, 8, 1, 2026, 10, 31));
        QCOMPARE(ReportRange::forPreset(Preset::SixMonths, today), range(2026, 5, 1, 2026, 10, 31));
        QCOMPARE(ReportRange::forPreset(Preset::Year, today), range(2025, 11, 1, 2026, 10, 31));
        QCOMPARE(ReportRange::forPreset(Preset::SixMonths, QDate(2026, 2, 28)), range(2025, 9, 1, 2026, 2, 28));
        QCOMPARE(ReportRange::forPreset(Preset::Year, today).months(), 12);
    }

    void customRangesAreTurnedRound() {
        QCOMPARE(ReportRange::custom(QDate(2026, 10, 15), QDate(2026, 9, 1)), range(2026, 9, 1, 2026, 10, 15));
        QVERIFY(!ReportRange::custom(QDate(2026, 9, 1), QDate(2026, 10, 15)).isWholeMonths());
        QVERIFY(ReportRange::custom(QDate(2026, 9, 1), QDate(2026, 10, 31)).isWholeMonths());
    }

    void comparesWithTheSameLengthBefore() {
        // Finished months: the whole months before
        QCOMPARE(range(2026, 4, 1, 2026, 9, 30).comparison(today), range(2025, 10, 1, 2026, 3, 31));
        // Ongoing: only up to the same day
        QCOMPARE(ReportRange::forPreset(Preset::SixMonths, today).comparison(today), range(2025, 11, 1, 2026, 4, 29));
        QCOMPARE(ReportRange::forPreset(Preset::Month, today).comparison(today), range(2026, 9, 1, 2026, 9, 29));
        QCOMPARE(ReportRange::forPreset(Preset::ThreeMonths, QDate(2026, 5, 31)).comparison(QDate(2026, 5, 31)),
                 range(2025, 12, 1, 2026, 2, 28));                       // Ends on the last day: finished
        QCOMPARE(ReportRange::forPreset(Preset::Month, QDate(2026, 3, 30)).comparison(QDate(2026, 3, 30)),
                 range(2026, 2, 1, 2026, 2, 28));                       // February has no 30th

        // Custom: the days before
        QCOMPARE(range(2026, 9, 1, 2026, 9, 10).comparison(today), range(2026, 8, 22, 2026, 8, 31));
        QCOMPARE(range(2026, 10, 20, 2026, 11, 5).comparison(today), range(2026, 10, 3, 2026, 10, 12));
    }

    void cutsIntoDaysWeeksAndMonths() {
        const ReportRange october = range(2026, 10, 1, 2026, 10, 31);
        QCOMPARE(october.periods(Unit::Day).size(), 31);
        const QList<ReportRange> weeks = october.periods(Unit::Week);
        QCOMPARE(weeks.size(), 5);
        QCOMPARE(weeks.first(), range(2026, 10, 1, 2026, 10, 4));      // Thursday to Sunday
        QCOMPARE(weeks[1], range(2026, 10, 5, 2026, 10, 11));
        QCOMPARE(weeks.last(), range(2026, 10, 26, 2026, 10, 31));
        const QList<ReportRange> months = range(2026, 9, 15, 2026, 11, 3).periods(Unit::Month);
        QCOMPARE(months.size(), 3);
        QCOMPARE(months.first(), range(2026, 9, 15, 2026, 9, 30));
        QCOMPARE(months.last(), range(2026, 11, 1, 2026, 11, 3));

        QCOMPARE(october.flowUnit(), Unit::Week);
        QCOMPARE(ReportRange::forPreset(Preset::ThreeMonths, today).flowUnit(), Unit::Month);
    }

    void labels() {
        QCOMPARE(range(2026, 10, 1, 2026, 10, 31).label(), QString("October 2026"));
        QCOMPARE(range(2026, 5, 1, 2026, 10, 31).label(), QString("May – October 2026"));
        QCOMPARE(range(2025, 11, 1, 2026, 10, 31).label(), QString("November 2025 – October 2026"));
        QCOMPARE(range(2026, 9, 1, 2026, 10, 15).label(), QString("1 Sep – 15 Oct 2026"));
        QCOMPARE(range(2025, 12, 20, 2026, 1, 6).label(), QString("20 Dec 2025 – 6 Jan 2026"));
        QCOMPARE(range(2026, 10, 3, 2026, 10, 3).label(), QString("3 Oct 2026"));
    }

    void presetKeys() {
        for (Preset preset : {Preset::Month, Preset::ThreeMonths, Preset::SixMonths, Preset::Year, Preset::Custom}) {
            QCOMPARE(ReportRange::presetFromKey(ReportRange::key(preset)), preset);
        }
        QVERIFY(!ReportRange::presetFromKey("weekly"));
    }
};

QTEST_MAIN(TestReport)
#include "test_report.moc"
