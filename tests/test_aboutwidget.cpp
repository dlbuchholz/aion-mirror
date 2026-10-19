#include <QtTest>
#include <QLabel>

#include "systeminfo.h"
#include "tests/test_db_fixture.h"
#include "widgets/aboutwidget.h"

class TestAboutWidget : public QObject {
    Q_OBJECT
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); }
    void cleanup() { db.close(); }

    void showsEveryEntry() {
        AboutWidget about;
        QStringList texts;
        for (QLabel* label : about.findChildren<QLabel*>()) texts.append(label->text());
        for (const SystemInfo::Entry& entry : SystemInfo::entries()) {
            QVERIFY2(texts.contains(entry.label), qPrintable(entry.label));
            QVERIFY2(texts.contains(entry.value), qPrintable(entry.value));
        }
    }

    void showingAgainKeepsTheRows() {
        // Rebuilding all labels on every show made the page slow to open
        AboutWidget about;
        const QList<QLabel*> before = about.findChildren<QLabel*>();
        about.show();
        about.hide();
        about.show();
        about.refresh();
        QCOMPARE(about.findChildren<QLabel*>(), before);
    }
};

QTEST_MAIN(TestAboutWidget)
#include "test_aboutwidget.moc"
