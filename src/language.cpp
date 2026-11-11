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
    return Language::isBuiltIn(code);
}

} // namespace

namespace Language {

QList<Option> available() {
    return {{"en", "English"}, {"de", "Deutsch"}};
}

bool isBuiltIn(const QString& code) {
#ifndef AION_HAS_TRANSLATIONS
    if (code != "en") return false;                  // Built without Qt's LinguistTools: English only
#endif
    for (const Language::Option& option : Language::available()) {
        if (option.code == code) return true;
    }
    return false;
}

QString resolve(const QString& setting, const QLocale& system) {
    if (setting != System && isAvailable(setting)) {
        return setting;
    }
    // The languages the system shows its own texts in, the preferred one first. Not
    // system.language(): on Windows that's the language of the regional format (dates, numbers),
    // which may be another one than the display language.
    for (const QString& name : system.uiLanguages()) {
        const QString code = QLocale::languageToCode(QLocale(name).language());
        if (isAvailable(code)) return code;
    }
    return QString("en");
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

QString restartProgram(const QString& applicationFilePath, const QString& appImage, const QString& appDir) {
    const bool insideAppImage = !appImage.isEmpty() && !appDir.isEmpty() && applicationFilePath.startsWith(appDir + '/');
    return insideAppImage ? appImage : applicationFilePath;
}

bool restartApplication() {
    const QString program = restartProgram(QCoreApplication::applicationFilePath(), qEnvironmentVariable("APPIMAGE"),
                                           qEnvironmentVariable("APPDIR"));
    if (!QProcess::startDetached(program, QCoreApplication::arguments().mid(1))) {
        qWarning() << "Aion couldn't be started again:" << program;
        return false;                                // Stays open, rather than just closing
    }
    QApplication::closeAllWindows();
    QCoreApplication::quit();
    return true;
}

} // namespace Language
