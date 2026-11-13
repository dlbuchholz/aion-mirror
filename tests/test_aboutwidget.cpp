#include <QtTest>
#include "appname.h"
#include <QAbstractButton>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>

#include "systeminfo.h"
#include "version.h"
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

    void theCopyButtonCopiesTheReport() {
        AboutWidget about;
        about.findChild<QAbstractButton*>("copyInfoButton")->click();
        QCOMPARE(QGuiApplication::clipboard()->text(), SystemInfo::report());
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

    void checkingForUpdatesSaysItIsntThereYet() {
        AboutWidget about;
        QAbstractButton* check = nullptr;
        for (QAbstractButton* button : about.findChildren<QAbstractButton*>()) {
            if (button->text() == "Check for updates") check = button;
        }
        QVERIFY(check);
        QString title;
        QString said;
        QTimer::singleShot(0, qApp, [&]() {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                title = box->windowTitle();
                said = box->text();
                box->accept();
            }
        });
        check->click();
        QCOMPARE(title, QString("Check for updates"));
        QCOMPARE(said, QString("You're using %1 %2.\n\nChecking for updates isn't available yet.").arg(APP_NAME, APP_VERSION));
    }
};

QTEST_MAIN(TestAboutWidget)
#include "test_aboutwidget.moc"
