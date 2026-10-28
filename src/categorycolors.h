#ifndef CATEGORYCOLORS_H
#define CATEGORYCOLORS_H

#include <QColor>
#include <QStringList>

// The palette categories can pick from. Categories store the name ("teal"); the actual
// colours depend on the light or dark theme so tags stay readable in both.
namespace CategoryColors {

struct Swatch {
    QColor background;                               // Tag fill
    QColor text;                                     // Text on the tag
    QColor solid;                                    // Bars, dots, the colour picker
};

QStringList names();                                 // In picker order, without the neutral default
QString label(const QString& name);                  // "Teal", or "No colour" for an empty name
Swatch swatch(const QString& name);                  // Unknown or empty names give the neutral look
Swatch swatch(const QString& name, bool dark);       // In the given theme rather than the active one

} // namespace CategoryColors

#endif // CATEGORYCOLORS_H
