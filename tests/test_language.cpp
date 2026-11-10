#include <QtTest>
#include <QXmlStreamReader>

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

    // What a translation needs to keep from the English: its placeholders (%1, %2, …) and HTML
    // tags, each as often as there. Their order may change with the language's word order.
    static QStringList mustKeep(const QString& text) {
        static const QRegularExpression pattern(R"(%\d+|</?[A-Za-z]+)");
        QStringList found;
        for (const QRegularExpressionMatch& match : pattern.globalMatch(text)) found << match.captured();
        found.sort();
        return found;
    }

    // Languages may be translated only in part: what's missing shows in English. But what is
    // translated keeps the English text's placeholders and tags, or values and links go missing.
    void translationsKeepPlaceholdersAndTags() {
        for (const Language::Option& language : Language::available()) {
            if (language.code == "en") continue;
            QFile file(QString(AION_TRANSLATIONS_DIR) + "/aion_" + language.code + ".ts");
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
            QXmlStreamReader xml(&file);
            QString context;
            QString source;
            QStringList broken;
            int messages = 0;
            while (!xml.atEnd()) {
                if (!xml.readNextStartElement()) continue;
                if (xml.name() == u"name") {
                    context = xml.readElementText();
                } else if (xml.name() == u"source") {
                    source = xml.readElementText();
                    ++messages;
                } else if (xml.name() == u"translation") {
                    const QString translation = xml.readElementText();
                    if (!translation.isEmpty() && mustKeep(translation) != mustKeep(source)) {
                        broken << QString("%1: \"%2\" → \"%3\"").arg(context, source, translation);
                    }
                }
            }
            QVERIFY2(!xml.hasError(), qPrintable(file.fileName() + ": " + xml.errorString()));
            QVERIFY2(messages > 0, qPrintable(file.fileName()));
            QVERIFY2(broken.isEmpty(), qPrintable(language.code + ":\n" + broken.join("\n")));
        }
    }

    void placeholdersAndTagsAreFoundInAnyOrder() {
        QCOMPARE(mustKeep("Delete %2 and %1 transactions"), mustKeep("%2 und %1 Umsätze löschen"));
        QCOMPARE(mustKeep("Expenses <b>%1</b>"), (QStringList{"%1", "</b", "<b"}));
        QVERIFY(mustKeep("%1 of %2") != mustKeep("%1 von"));                    // A value lost
        QVERIFY(mustKeep("%1 of %2") != mustKeep("%1 von %1"));                 // or shown twice
        QVERIFY(mustKeep("Net <b>%1</b>") != mustKeep("Saldo %1"));             // Bold lost
        QVERIFY(mustKeep("%10 items") != mustKeep("%1 items"));                 // Two digits are one placeholder
    }

    void germanIsReallyGerman() {
#ifndef AION_HAS_TRANSLATIONS
        QSKIP("Built without Qt's LinguistTools");
#endif
        QVERIFY(Language::install("de"));
        QCOMPARE(QCoreApplication::translate("SettingsPage", "Language"), QString("Sprache"));
        QCOMPARE(QCoreApplication::translate("TransactionsPage", "%1 transactions"), QString("%1 Umsätze"));
        QCOMPARE(QCoreApplication::translate("Category", "Transfer"), QString("Umbuchung"));
        QCOMPARE(QCoreApplication::translate("Backup", "Scheduled"), QString("Geplant"));
    }

    void aLanguageWithoutTranslationsStaysEnglish() {
        QVERIFY(!Language::install("xx"));
        QCOMPARE(Language::current(), QString("en"));
    }
};

QTEST_MAIN(TestLanguage)
#include "test_language.moc"
