#include "language.h"
#include "translationprogress.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QProcess>
#include <QTranslator>
#include <memory>

namespace {

QString installed = "en";
std::unique_ptr<QTranslator> aionTranslator;
std::unique_ptr<QTranslator> qtTranslator;

void removeTranslators() {
    for (std::unique_ptr<QTranslator>* translator : {&aionTranslator, &qtTranslator}) {
        if (*translator) {
            QCoreApplication::removeTranslator(translator->get());
            translator->reset();
        }
    }
}

bool isAvailable(const QString& code) {
    for (const Language::Option& option : Language::available()) {
        if (option.code == code) return true;
    }
    return false;
}

} // namespace

namespace Language {

QList<Option> available() {
    return {{"en", "English"}, {"de", "Deutsch"}};
}

QString resolve(const QString& setting, const QLocale& system) {
    if (setting != System && isAvailable(setting)) {
        return setting;
    }
    const QString systemCode = QLocale::languageToCode(system.language());
    return isAvailable(systemCode) ? systemCode : QString("en");
}

bool install(const QString& code) {
    removeTranslators();
    installed = "en";
    if (code == "en") {
        return true;
    }

    auto aion = std::make_unique<QTranslator>();
    if (!isAvailable(code) || !aion->load(":/i18n/aion_" + code)) {
        qWarning() << "No translations for the language" << code << "- showing English";
        return false;
    }
    QCoreApplication::installTranslator(aion.get());
    aionTranslator = std::move(aion);

    // Qt's own texts (standard buttons, context menus): built in when the build had them, else
    // next to Qt
    auto qt = std::make_unique<QTranslator>();
    if (qt->load(":/i18n/qtbase_" + code) || qt->load("qtbase_" + code, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(qt.get());
        qtTranslator = std::move(qt);
    }
    installed = code;
    return true;
}

QString current() {
    return installed;
}

int percentTranslated(const QString& code) {
    QList<Progress> built;                           // As counted when Aion was built
    for (const TranslationProgress& progress : TranslationProgresses) {
        if (progress.code[0] != '\0') built.append({QString::fromLatin1(progress.code), progress.translated, progress.total});
    }
    return percentTranslated(code, built);
}

int percentTranslated(const QString& code, const QList<Progress>& progress) {
    if (code == "en") return 100;
    for (const Progress& language : progress) {
        if (language.code == code) return percentOf(language.translated, language.total);
    }
    return 0;
}

int percentOf(int translated, int total) {
    return total > 0 ? int(qint64(translated) * 100 / total) : 0;
}

void restartApplication() {
    QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
    QApplication::closeAllWindows();
    QCoreApplication::quit();
}

} // namespace Language
