#include <QSettings>
#include <QTemporaryDir>

// Linked into every test, so none of them can touch the settings of the real app. Set up
// before main(): Settings is created later, on first use, and picks up this format and path.
// Each test program gets its own folder, gone when it ends, however its tests went.
static QTemporaryDir settingsFolder;
static const bool settingsIsolated = []() {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsFolder.path());
    return true;
}();
