#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QList>
#include <QString>

// Technical information for the About page and bug reports
namespace SystemInfo {

struct Entry {
    QString section;                                 // "Aion", "System", "Data"
    QString label;
    QString value;
};

QList<Entry> entries();

// "0.1.0", or "0.1.0 (debug 450f44e)" in debug builds so testers can tell builds apart
QString versionText();

// Plain-text report for the clipboard. The home folder is replaced with "~" so pasted
// reports don't reveal the user name.
QString report();

QString maskHomePath(const QString& text);

} // namespace SystemInfo

#endif // SYSTEMINFO_H
