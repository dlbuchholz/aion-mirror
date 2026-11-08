#include <QtTest>

#include "language.h"

class TestLanguage : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { Language::install("en"); }

    void englishFirstThenGerman() {
        const QList<Language::Option> languages = Language::available();
        QVERIFY(languages.size() >= 2);
        QCOMPARE(languages.at(0).code, QString("en"));
        QCOMPARE(languages.at(0).nativeName, QString("English"));
        QCOMPARE(languages.at(1).code, QString("de"));
        QCOMPARE(languages.at(1).nativeName, QString("Deutsch"));
    }

    void aChosenLanguageWins() {
        QCOMPARE(Language::resolve("de", QLocale(QLocale::English, QLocale::UnitedKingdom)), QString("de"));
        QCOMPARE(Language::resolve("en", QLocale(QLocale::German, QLocale::Germany)), QString("en"));
    }

    void systemFollowsTheComputerWhenAionHasItsLanguage() {
        QCOMPARE(Language::resolve(Language::System, QLocale(QLocale::German, QLocale::Germany)), QString("de"));
        QCOMPARE(Language::resolve(Language::System, QLocale(QLocale::German, QLocale::Austria)), QString("de"));
        QCOMPARE(Language::resolve(Language::System, QLocale(QLocale::English, QLocale::UnitedStates)), QString("en"));
        QCOMPARE(Language::resolve(Language::System, QLocale(QLocale::French, QLocale::France)), QString("en"));
    }

    void anUnknownSettingFollowsTheSystem() {
        QCOMPARE(Language::resolve("xx", QLocale(QLocale::German, QLocale::Germany)), QString("de"));
        QCOMPARE(Language::resolve("", QLocale(QLocale::French, QLocale::France)), QString("en"));
    }

    void startsInEnglish() {
        QCOMPARE(Language::current(), QString("en"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QString("Cancel"));
    }

    void germanAndBackToEnglish() {
#ifndef AION_HAS_TRANSLATIONS
        QSKIP("Built without Qt's LinguistTools");
#endif
        QVERIFY(Language::install("de"));
        QCOMPARE(Language::current(), QString("de"));
        // Qt's own texts come along, so standard buttons are German too
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QString("Abbrechen"));

        QVERIFY(Language::install("en"));
        QCOMPARE(Language::current(), QString("en"));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QString("Cancel"));
    }

    void aLanguageWithoutTranslationsStaysEnglish() {
        QVERIFY(!Language::install("xx"));
        QCOMPARE(Language::current(), QString("en"));
    }
};

QTEST_MAIN(TestLanguage)
#include "test_language.moc"
