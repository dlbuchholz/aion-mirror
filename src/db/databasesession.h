#ifndef DATABASESESSION_H
#define DATABASESESSION_H

#include <QString>

// Which database file Aion works on. Every switch goes through open()/create(), which
// also run the migrations and remember the file in the recent list. This is the place
// a password prompt would go if databases get encrypted later.
namespace DatabaseSession {

QString defaultDatabasePath();                       // AppData/.../app_data.db
QString currentDatabasePath();                       // Empty while no database is open

// Opens an existing Aion database (or creates the default one). On failure the previously
// open database stays active and errorMessage says why.
bool open(const QString& path, QString* errorMessage = nullptr);

// Opens the database used last time, or the default one if there's none or it fails. In that
// failure warningMessage says what happened, for the user to see. errorMessage is for when
// neither could be opened.
bool openAtStartup(QString* warningMessage = nullptr, QString* errorMessage = nullptr);

// Creates a new, empty Aion database. Refuses to touch an existing file.
bool create(const QString& path, QString* errorMessage = nullptr);

} // namespace DatabaseSession

#endif // DATABASESESSION_H
