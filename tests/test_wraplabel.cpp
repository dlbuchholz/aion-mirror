#include <QtTest>

#include "widgets/wraplabel.h"

class TestWrapLabel : public QObject {
    Q_OBJECT

    static int advance(const WrapLabel& label, const QString& text) {
        return label.fontMetrics().boundingRect(QRect(0, 0, 100000, 100000), Qt::TextSingleLine, text).width();
    }

    // As high as the label is with all the room it wants
    static int oneLine(const WrapLabel& label) { return label.heightForWidth(100000); }

private slots:
    void asksForTheWholeLine() {
        WrapLabel label("No income this month yet");
        QCOMPARE(label.sizeHint().width(), advance(label, "No income this month yet"));
        QCOMPARE(label.sizeHint().height(), oneLine(label));
        // Given that, it stays on one line
        QCOMPARE(label.heightForWidth(label.sizeHint().width()), oneLine(label));
    }

    // Whatever the text and the font's fractions of a pixel: given what it asks for, it's one line
    void whatItAsksForIsAlwaysEnoughForOneLine() {
        for (const QString& text : {"No change this month", "Keine Veränderung in diesem Monat", "Spent per day so far",
                                    "0 transactions in November", "Noch keine Einnahmen in diesem Monat", "iiiiil llll",
                                    "WWW MMM", "−12% vs. Oktober"}) {
            for (const qreal points : {8.25, 9.0, 9.75, 11.5}) {
                WrapLabel label(text);
                QFont font = label.font();
                font.setPointSizeF(points);
                label.setFont(font);
                QVERIFY2(label.heightForWidth(label.sizeHint().width()) == oneLine(label), qPrintable(text));
                // Its longest word doesn't break either
                QVERIFY2(label.minimumSizeHint().width() >= advance(label, text.split(' ').first()), qPrintable(text));
            }
        }
    }

    void wrapsWhenNarrower() {
        WrapLabel label("No income this month yet");
        QVERIFY(label.heightForWidth(label.sizeHint().width() / 2) > oneLine(label));
        QCOMPARE(oneLine(label), WrapLabel("One").heightForWidth(100000));
    }

    void neverAsksForLessThanItsLongestWord() {
        WrapLabel label("Noch keine Einnahmen in diesem Monat");
        QCOMPARE(label.minimumSizeHint().width(), advance(label, "Einnahmen"));
        QVERIFY(label.minimumSizeHint().width() < label.sizeHint().width());
    }

    // Layouts ask for the height at the width they give; as high as at its narrowest, it would
    // leave a gap below it whenever there's room
    void itsMinimumIsOneLineHigh() {
        WrapLabel label("Noch keine Einnahmen in diesem Monat");
        QCOMPARE(label.minimumSizeHint().height(), oneLine(label));
    }

    void marginsCount() {
        WrapLabel label("No expenses this month yet.");
        label.setContentsMargins(6, 8, 7, 9);
        label.setMargin(2);
        QCOMPARE(label.sizeHint().width(), advance(label, "No expenses this month yet.") + 6 + 7 + 4);
        QCOMPARE(label.minimumSizeHint().width(), advance(label, "expenses") + 6 + 7 + 4);
        QCOMPARE(label.heightForWidth(label.sizeHint().width()), oneLine(label));
        QCOMPARE(label.sizeHint().height(), oneLine(label));
        QVERIFY(oneLine(label) >= label.fontMetrics().height() + 8 + 9 + 4);
    }

    void anEmptyLabelAsksForNoWidth() {
        WrapLabel label;
        QVERIFY(label.wordWrap());
        QCOMPARE(label.sizeHint().width(), 0);
        QCOMPARE(label.minimumSizeHint().width(), 0);
    }

    void followsItsText() {
        WrapLabel label("Short");
        const int before = label.sizeHint().width();
        label.setText("Something much longer than before");
        QVERIFY(label.sizeHint().width() > before);
    }
};

QTEST_MAIN(TestWrapLabel)
#include "test_wraplabel.moc"
