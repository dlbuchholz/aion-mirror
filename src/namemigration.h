#ifndef NAMEMIGRATION_H
#define NAMEMIGRATION_H

#include <QString>
#include <QVariant>

// The program was called Aion before. Its data folder and settings carry that name, so on the
// first start under the new name they're copied over: the data folder (the default database and
// its backups) and every setting, with paths that pointed into the old folder now pointing into
// the new one. The old folder and settings stay as they are, as a backup.
namespace NameMigration {

inline constexpr char PreviousName[] = "Aion";

// Where the old and the new name keep their data and settings
struct Places {
    QString oldDataFolder;
    QString newDataFolder;
    QString organization;                            // QSettings' organization, the same for both
    QString oldApplication;                          // QSettings' application names
    QString newApplication;
};

// The real places, in QStandardPaths' app data folder and QSettings' default format. Call after
// QCoreApplication::setApplicationName(APP_NAME).
Places standardPlaces();

enum class Outcome {
    NotNeeded,                                       // Nothing under the old name, or the new one is in use already
    CarriedOver,
    Failed,                                          // The new settings stay empty, so the next start tries again
};

// Copies the old data folder and settings, once: only while the new settings are empty. A failure
// says why in error.
Outcome carryOver(const Places& places, QString* error = nullptr);

// The value with every path in oldFolder (or the folder itself) moved to newFolder: strings,
// string lists, and the keys and values of maps, at any depth. Other values come back unchanged.
QVariant movedPaths(const QVariant& value, const QString& oldFolder, const QString& newFolder);

} // namespace NameMigration

#endif // NAMEMIGRATION_H
