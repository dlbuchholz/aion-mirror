#include <QtTest>
#include <QLabel>
#include <QSvgRenderer>

#include "icons.h"
#include "startwindow.h"
#include "systeminfo.h"
#include "tests/test_db_fixture.h"
#include "widgets/aboutwidget.h"

class TestBranding : public QObject {
    Q_OBJECT
    TestDbFixture db;

private slots:
    void initTestCase() { db.init(); }
    void init() { db.yankDb(); }
    void cleanup() { db.close(); }

    void logoIsBuiltIn() {
        QVERIFY(QSvgRenderer(QString(":/aion-logo.svg")).isValid());
        const QImage logo = Icons::logo(64, 1.0).toImage();
        QCOMPARE(logo.size(), QSize(64, 64));
        QCOMPARE(logo.pixelColor(32, 32).alpha(), 255);                     // The tile is solid
        QCOMPARE(logo.pixelColor(0, 0).alpha(), 0);                         // Its corners are rounded off

        const QImage sharp = Icons::logo(64, 2.0).toImage();                // High-DPI screens get more pixels
        QCOMPARE(sharp.size(), QSize(128, 128));
    }

    void appIconHasTheSizesWindowsAsksFor() {
        const QList<QSize> sizes = Icons::appIcon().availableSizes();
        for (int size : {16, 24, 32, 48, 256}) {
            QVERIFY2(sizes.contains(QSize(size, size)), qPrintable(QString("missing %1 px").arg(size)));
        }
    }

    void startWindowAndAboutShowTheLogo() {
        StartWindow start;
        auto* startLogo = start.findChild<QLabel*>("logoLabel");
        QVERIFY(startLogo);
        QVERIFY(!startLogo->pixmap().isNull());

        AboutWidget about;
        QVERIFY(!about.findChild<QLabel*>("logoLabel")->pixmap().isNull());
        QCOMPARE(about.findChild<QLabel*>("aboutVersionLabel")->text(), QString("Version %1").arg(SystemInfo::versionText()));
        QVERIFY(about.findChild<QLabel*>("aboutLicenseLabel")->text().contains("GPL, version 3 or later"));
    }
};

QTEST_MAIN(TestBranding)
#include "test_branding.moc"
