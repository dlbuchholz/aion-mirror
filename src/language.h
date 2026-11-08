#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QList>
#include <QLocale>
#include <QString>

// The language of Aion's texts. English is written in the code; the others come from
// translations/aion_<code>.ts, built into the program (:/i18n/aion_<code>.qm), together with Qt's
// own texts (standard buttons) when that build had them. Set once at start: a change in the
// settings takes effect when Aion starts again.
namespace Language {

// The stored setting that follows the system's language
inline const QString System = "system";

struct Option {
    QString code;           // "en", "de"
    QString nativeName;     // In its own language, "Deutsch", so it can be found in any language
};

// The languages Aion can show, English first
QList<Option> available();

// The language for a stored setting: the one chosen, or for "system" (or anything unknown) the
// system's language when Aion has it, else English
QString resolve(const QString& setting, const QLocale& system = QLocale::system());

// Shows Aion's texts in this language from now on. English, or a language without
// translations, removes them. Returns whether the language is now active.
bool install(const QString& code);

// The language installed last, "en" before any
QString current();

// Starts Aion again with the same arguments and quits this one
void restartApplication();

} // namespace Language

#endif // LANGUAGE_H
