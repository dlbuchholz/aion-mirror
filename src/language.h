#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QList>
#include <QLocale>
#include <QString>

// The language of Liora's texts. English is written in the code; the others come from
// translations/liora_<code>.ts, built into the program (:/i18n/liora_<code>.qm), together with Qt's
// own texts (standard buttons) when that build had them. Set once at start: a change in the
// settings takes effect when Liora starts again.
namespace Language {

// The stored setting that follows the system's language
inline const QString System = "system";

struct Option {
    QString code;           // "en", "de"
    QString nativeName;     // In its own language, "Deutsch", so it can be found in any language
};

// The languages Liora can show, English first
QList<Option> available();

// Whether Liora can show a language: English always, the others only when this build has the
// translations (built with Qt's LinguistTools). One it can't show isn't offered or followed.
bool isBuiltIn(const QString& code);

// The language for a stored setting: the one chosen, or for "system" (or anything unknown) the
// first of the system's display languages that Liora has, else English
QString resolve(const QString& setting, const QLocale& system = QLocale::system());

// Shows Liora's texts in this language from now on. English, or a language without
// translations, removes them. Returns whether the language is now active.
bool install(const QString& code);

// The language installed last, "en" before any
QString current();

// How much of a language is translated, in whole percent rounded down, so only a complete
// translation is 100: English always, a language Liora doesn't have 0. Counted from its .ts file
// when Liora was built. Untranslated texts show in English.
int percentTranslated(const QString& code);

// The same from a list of counts, one per language
struct Progress {
    QString code;
    int translated = 0;
    int total = 0;
};
int percentTranslated(const QString& code, const QList<Progress>& progress);

// The share in whole percent, rounded down; 0 when there's nothing to translate
int percentOf(int translated, int total);

// Starts Liora again with the same arguments and quits this one. Returns false, and leaves this
// one open, when it couldn't be started.
bool restartApplication();

// The program that starts Liora again: the running one, or the AppImage file when it runs from
// inside one (the AppImage sets APPIMAGE and APPDIR; its mounted program goes when it ends).
// Only when the program is in that APPDIR: both are inherited from an AppImage that started Liora.
QString restartProgram(const QString& applicationFilePath, const QString& appImage, const QString& appDir);

} // namespace Language

#endif // LANGUAGE_H
