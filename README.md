# <img src="assets/aion-logo.svg" alt="" width="48" align="middle"> Aion

[![Coverage](docs/coverage-badge.svg)](COVERAGE.md)
[![License: GPL-3.0-or-later](docs/license-badge.svg)](LICENSE)

A personal finance app for the desktop.
Built with Qt 6 Widgets and SQLite.

Abandon Excel. I made this app to suit my workflow and my needs, maybe it suits yours too.

> [!WARNING]
> Aion is a very early build, still in development. Expect missing features and bugs, and maybe don't
> rely on it for daily use just yet. Please report bugs on the
> [issues page](https://codeberg.org/spectre_0/aion/issues).

- [Test coverage](COVERAGE.md): which files are covered by tests
- [Roadmap](ROADMAP.md): what's planned

**What it does:**

- **Accounts and transactions:** keep several accounts, add and edit transactions, and correct a
  balance when it's off.
- **Import:** read exports from your bank as CSV, Excel or LibreOffice files.
- **Rules:** set categories and clean up payee names automatically.
- **Budgets:** set monthly limits per category.
- **Dashboard:** see your balance, income, expenses, savings and spending by category at a glance.
- **Reports:** look at spending and budgets over a date range.
- **Backups:** back up automatically and restore from inside the app. Everything stays on your
  own computer, there is no online service involved.

Aion tries to follow the ideas of [permacomputing](https://permacomputing.net/): use as little
computing, energy and storage as the job needs, do work only when it's needed, and keep things
small, simple and long-lived. Feel free to suggest anything related to that.

## Download and install

Aion runs on Windows and Linux (64-bit). Get it from the
[releases page](https://codeberg.org/spectre_0/aion/releases).

**Windows:** run the file ending in `-setup.exe`. It does not need administrator rights,
 you can install it on your user account only. There's also a `.zip` if you'd
 rather not install anything: unpack it and start `Aion.exe`.

**Linux:**

| System | File | How |
|---|---|---|
| Fedora 44 (or other RPM-based systems) | `.rpm` | `sudo dnf install ./<file>.rpm` |
| Debian 13 (or other DEB-based systems) | `.deb` | `sudo apt install ./<file>.deb` |
| Anything else | `.AppImage` | Make it executable (`chmod +x`) and start it |

The `.rpm` and `.deb` use your system's Qt. They're built and checked on the release named in the
file, and may work on similar systems if those have Qt 6.8 or newer.
If one won't install, use the AppImage, which brings its own Qt and also runs
on older systems.

Aion doesn't update itself yet. To update, download the new version
from this repository. Your data stays where it is, and a database is backed up
before a new version changes it.

## Your data

Aion keeps everything on your computer:

| | Windows | Linux |
|---|---|---|
| Database | `%APPDATA%\Aion\app_data.db` | `~/.local/share/Aion/app_data.db` |
| Settings | Registry, `HKCU\Software\LeineHive\Aion` | `~/.config/LeineHive/Aion.conf` |

If you create or open a database somewhere else, Aion uses that file instead. Settings > About shows
the folders in use.

Automatic backups go in a `backups` folder next to the database. Settings > Backup sets how often
they run, and you can restore them or save a copy elsewhere from there. They're on the same disk as
the database, so they won't help if that disk fails. Keep a copy somewhere else too.

**Nothing is encrypted yet. I am still investigating at-rest encryption.**
The database and its backups are ordinary SQLite files, and anyone who can
read them can read your finances. To protect them, use your system's disk
encryption, and take care where you put copies.

Uninstalling on Windows keeps your data unless you tick "My data". That removes the default database
folder, including the backups in it, and your settings. Databases in other folders are kept. On
Linux, removing the package or deleting the AppImage keeps your data. Delete the paths above if you
want it gone too.

## Building

Needs Qt 6.5 or newer and CMake 3.19 or newer (newer Qt versions ask for more: Qt 6.12 needs 3.25). Open `CMakeLists.txt` in Qt Creator, or:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=<path to Qt>
cmake --build build
ctest --test-dir build
```

To set up your development environment (CLion or Qt Creator) and to pick a compiler that matches your Qt kit see [CONTRIBUTING.md](CONTRIBUTING.md).

The coverage report is measured nightly by CI. To measure it yourself (GCC or Clang, with
[gcovr](https://gcovr.com)):

```
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DAION_COVERAGE=ON -DCMAKE_PREFIX_PATH=<path to Qt>
cmake --build build-coverage
scripts/coverage.sh build-coverage
```

`--publish` also rewrites `COVERAGE.md` and the badge.

Every push is built and tested for Linux and Windows on a CI runner dedicated to Aion and run by me (its
maintainer), so the builds don't use Codeberg's CI resources. How it works:
[Builds and releases](CONTRIBUTING.md#builds-and-releases).

## License

Copyright © 2024–2026 the Aion contributors (see [AUTHORS](AUTHORS)).

Aion is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3 of the License, or (at
your option) any later version. It is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; see [LICENSE](LICENSE) for details.

This covers the source code and every asset made for Aion, except its name and logo.

**Name and logo.** The name "Aion" and the Aion logo (`assets/aion-logo.svg` and the icons made from
it) are not covered by the GPL; all rights reserved by the Aion contributors. You may use them to refer
to Aion, for example in articles or links. Modified versions that get distributed need their own name and
logo.

**Third-party parts** keep their own licenses:

| What | License |
|---|---|
| Interface icons in `assets/icons/`, based on [Tabler Icons](https://tabler.io/icons) | MIT, see [assets/icons/LICENSE](assets/icons/LICENSE) |
| [miniz](https://github.com/richgel999/miniz) in `thirdparty/miniz/`, for reading spreadsheets | MIT, see [thirdparty/miniz/LICENSE](thirdparty/miniz/LICENSE) |
| [Qt](https://www.qt.io) (not in this repository, bundled in the Windows downloads and the AppImage) | LGPL-3.0, see [LICENSE-LGPL](LICENSE-LGPL) |
| [SQLite](https://sqlite.org) (through Qt, bundled in the Windows downloads and the AppImage) | Public domain |

Those downloads also list every library they bundle, with its source, in `THIRD-PARTY`.
