#include "namemigration.h"
#include "appname.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

namespace {

// In the new settings once they're copied, naming the old application
constexpr char MigratedKey[] = "app/migratedFrom";

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity PathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity PathCase = Qt::CaseSensitive;
#endif

QString movedPath(const QString& path, const QString& oldFolder, const QString& newFolder) {
    if (path.compare(oldFolder, PathCase) == 0) {
        return newFolder;
    }
    if (path.startsWith(oldFolder + '/', PathCase)) {
        return newFolder + path.mid(oldFolder.size());
    }
    return path;
}

bool isEmptyFolder(const QString& path) {
    return QDir(path).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
}

// Copies everything in from (hidden files and subfolders too) into to
bool copyFolder(const QString& from, const QString& to, QString* error) {
    if (!QDir().mkpath(to)) {
        *error = QString("Couldn't create %1.").arg(QDir::toNativeSeparators(to));
        return false;
    }
    const QDir source(from);
    QDirIterator it(from, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo entry = it.nextFileInfo();
        const QString target = to + '/' + source.relativeFilePath(entry.filePath());
        if (entry.isDir()) {
            if (!QDir().mkpath(target)) {
                *error = QString("Couldn't create %1.").arg(QDir::toNativeSeparators(target));
                return false;
            }
        } else if (!QFile::copy(entry.filePath(), target)) {
            *error = QString("Couldn't copy %1.").arg(QDir::toNativeSeparators(entry.filePath()));
            return false;
        }
    }
    return true;
}

} // namespace

namespace NameMigration {

Places standardPlaces() {
    const QString name = QCoreApplication::applicationName();
    const QString newFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setApplicationName(PreviousName);  // The folder as the old name had it
    const QString oldFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setApplicationName(name);
    return {QDir::cleanPath(oldFolder), QDir::cleanPath(newFolder), APP_ORGANIZATION, PreviousName, APP_NAME};
}

QVariant movedPaths(const QVariant& value, const QString& oldFolder, const QString& newFolder) {
    switch (value.typeId()) {
    case QMetaType::QString:
        return movedPath(value.toString(), oldFolder, newFolder);
    case QMetaType::QStringList: {
        QStringList moved;
        for (const QString& item : value.toStringList()) moved << movedPath(item, oldFolder, newFolder);
        return moved;
    }
    case QMetaType::QVariantList: {
        QVariantList moved;
        for (const QVariant& item : value.toList()) moved << movedPaths(item, oldFolder, newFolder);
        return moved;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        QVariantMap moved;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            moved.insert(movedPath(it.key(), oldFolder, newFolder), movedPaths(it.value(), oldFolder, newFolder));
        }
        return moved;
    }
    default:
        return value;
    }
}

Outcome carryOver(const Places& places, QString* error) {
    QString ignored;
    QString& reason = error ? *error : ignored;

    QSettings newSettings(QSettings::defaultFormat(), QSettings::UserScope, places.organization, places.newApplication);
    if (!newSettings.allKeys().isEmpty()) {
        return Outcome::NotNeeded;                   // Carried over before, or used under the new name already
    }
    QSettings oldSettings(QSettings::defaultFormat(), QSettings::UserScope, places.organization, places.oldApplication);
    const QStringList keys = oldSettings.allKeys();
    const bool oldData = QFileInfo(places.oldDataFolder).isDir() && !isEmptyFolder(places.oldDataFolder);
    if (keys.isEmpty() && !oldData) {
        return Outcome::NotNeeded;                   // Never used under the old name
    }

    // The folder first, under a temporary name, so a half copy is never taken for the real one.
    // A new folder with files in it already is from an earlier start that stopped before the
    // settings were saved: it's kept, and the settings are copied now.
    if (oldData && (!QFileInfo::exists(places.newDataFolder) || isEmptyFolder(places.newDataFolder))) {
        const QString partial = places.newDataFolder + ".partial";
        QDir(partial).removeRecursively();           // Left over from a start that stopped while copying
        if (!copyFolder(places.oldDataFolder, partial, &reason)) {
            QDir(partial).removeRecursively();
            return Outcome::Failed;
        }
        QDir().rmdir(places.newDataFolder);          // Only if it's there and empty
        if (!QDir().rename(partial, places.newDataFolder)) {
            QDir(partial).removeRecursively();
            reason = QString("Couldn't create %1.").arg(QDir::toNativeSeparators(places.newDataFolder));
            return Outcome::Failed;
        }
    }

    for (const QString& key : keys) {
        newSettings.setValue(key, movedPaths(oldSettings.value(key), places.oldDataFolder, places.newDataFolder));
    }
    newSettings.setValue(MigratedKey, places.oldApplication);
    newSettings.sync();
    if (newSettings.status() != QSettings::NoError) {
        reason = QString("Couldn't save the settings in %1.").arg(newSettings.fileName());
        newSettings.clear();                         // Empty again, so the next start tries again
        newSettings.sync();
        return Outcome::Failed;
    }
    return Outcome::CarriedOver;
}

} // namespace NameMigration
