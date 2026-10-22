#include <QtTest>

#include "commandline.h"
#include "systeminfo.h"

class TestCommandLine : public QObject {
    Q_OBJECT

    static int run(const QStringList& options, QString* output) {
        QTextStream out(output);
        return CommandLine::handle(QStringList{"Aion"} + options, out);
    }

private slots:
    void versionPrintsTheVersionAndExits() {
        QString output;
        QCOMPARE(run({"--version"}, &output), 0);
        QCOMPARE(output, QString("Aion %1\n").arg(SystemInfo::versionText()));
    }

    void helpListsTheOptionsAndExits() {
        QString output;
        QCOMPARE(run({"--help"}, &output), 0);
        QVERIFY(output.contains("--version"));
        QVERIFY(output.contains("--help"));
        output.clear();
        QCOMPARE(run({"--help-all"}, &output), 0);
        QVERIFY(output.contains("--version"));
    }

    void noOptionsStartsNormally() {
        QString output;
        QCOMPARE(run({}, &output), -1);
        QVERIFY(output.isEmpty());
    }

    void unknownOptionsDontStopTheStart() {
        // Qt's own options, or a typo, start Aion as usual
        QString output;
        QCOMPARE(run({"-style", "fusion"}, &output), -1);
        QCOMPARE(run({"--poggers"}, &output), -1);
        QCOMPARE(run({"--versoin"}, &output), -1);
        QVERIFY(output.isEmpty());
    }
};

QTEST_MAIN(TestCommandLine)
#include "test_commandline.moc"
