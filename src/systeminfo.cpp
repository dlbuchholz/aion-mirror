#include "systeminfo.h"
#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "theme.h"
#include "version.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QScreen>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStyle>
#include <QSysInfo>

namespace {

QString schemaVersion() {
    QSqlQuery query(DatabaseManager::instance().database());
    if (query.exec("SELECT MAX(version) FROM schema_migrations") && query.next() && !query.value(0).isNull()) {
        return query.value(0).toString();
    }
    return "unknown";
}

QString buildDescription() {
    const QString buildType = QString(AION_BUILD_TYPE).isEmpty() ? QString("unknown") : QString(AION_BUILD_TYPE);
    return QString("%1 · %2").arg(buildType, QString(AION_COMPILER).trimmed());
}

QString displayDescription() {
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return "unknown";
    }
    return QString("%1 × %2, scale %3")
        .arg(screen->size().width())
        .arg(screen->size().height())
        .arg(screen->devicePixelRatio());
}

} // namespace

namespace SystemInfo {

QString versionText() {
#ifdef QT_NO_DEBUG
    return AION_VERSION;
#else
    const QString commit = AION_GIT_COMMIT;
    return commit == "unknown" ? QString("%1 (debug)").arg(AION_VERSION)
                               : QString("%1 (debug %2)").arg(AION_VERSION, commit);
#endif
}

QList<Entry> entries() {
    const QString databasePath = DatabaseSession::currentDatabasePath();
    const QFileInfo database(databasePath);
    const QString theme = Settings::instance().getTheme();

    return {
        {"Aion", "Version", AION_VERSION},
        {"Aion", "Commit", AION_GIT_COMMIT},
        {"Aion", "Build", buildDescription()},

        {"System", "Operating system", QString("%1 (kernel %2)").arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion())},
        {"System", "Architecture", QSysInfo::currentCpuArchitecture()},
        {"System", "Qt", QString("%1 (built with %2)").arg(qVersion(), QT_VERSION_STR)},
        {"System", "Platform", QString("%1, style %2").arg(QGuiApplication::platformName(),
                                                           qApp ? qApp->style()->name() : QString("unknown"))},
        {"System", "Display", displayDescription()},
        {"System", "Theme", QString("%1 (showing %2)").arg(theme, Theme::isDark() ? "dark" : "light")},
        {"System", "Locale", QLocale::system().name()},

        {"Data", "Database", QDir::toNativeSeparators(databasePath)},
        {"Data", "Database size", database.exists() ? QLocale(QLocale::English).formattedDataSize(database.size()) : QString("not found")},
        {"Data", "Schema version", schemaVersion()},
        {"Data", "Settings", QDir::toNativeSeparators(Settings::instance().getQSettings().fileName())},
        {"Data", "App data folder", QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))},
    };
}

QString maskHomePath(const QString& text) {
    QString masked = text;
    const QString home = QDir::homePath();
    for (const QString& form : {QDir::toNativeSeparators(home), home}) {
        masked.replace(form, "~", Qt::CaseInsensitive);
    }
    return masked;
}

QString report() {
    QString text;
    QString currentSection;
    for (const Entry& entry : entries()) {
        if (entry.section != currentSection) {
            if (!currentSection.isEmpty()) {
                text += '\n';
            }
            currentSection = entry.section;
            text += currentSection + '\n';
        }
        text += QString("  %1: %2\n").arg(entry.label, entry.value);
    }
    return maskHomePath(text);
}

} // namespace SystemInfo
