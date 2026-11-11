# Contributing to Aion

This page explains how to set up a development environment. For a quick build from the command line,
see [Building](README.md#building) in the README.

## What you need

- Qt 6.5 or newer, with the **Svg** module, from the [Qt installer](https://www.qt.io/download-qt-installer)
- CMake 3.19 or newer (Qt 6.12 needs 3.25) and Ninja. CLion and Qt Creator bundle both, and the Qt installer has them under
  `Qt\Tools`.
- A C++ compiler that **matches your Qt kit** (see below)

## Choosing a compiler (Windows)

Aion builds with MSVC and with MinGW. CI builds Linux and a MinGW build for Windows, and the coverage
script needs GCC or Clang. Use whichever you like. The one rule is that the compiler has to match the Qt kit
you build against, because Qt's libraries only link with the compiler they were built with:

| Qt kit | Compiler |
|---|---|
| `msvc2022_64` | Visual Studio 2022 or the VS 2022 Build Tools (x64) |
| `mingw_64` | The MinGW from the Qt installer (`Qt\Tools\mingw1310_64` for Qt 6.12) |

If the two don't match, configuring still works, but linking fails with hundreds of
`undefined reference to __imp__...` errors (MinGW against an MSVC kit) or `LNK2019` errors (MSVC
against a MinGW kit). CLion's bundled MinGW is a different version from the one Qt was built with,
so for the `mingw_64` kit, use the MinGW from the Qt installer.

## CLion

These steps use CLion's settings at **File → Settings → Build, Execution, Deployment**. In the paths
below, replace `C:\Qt\6.12.0\msvc2022_64` with your own Qt kit.

### 1. Toolchain

Under **Toolchains**, add a toolchain for your compiler and move it to the top of the list with ▲,
so it becomes the default:

- **MSVC:** **+ → Visual Studio**. Set **Toolset** to your Visual Studio or Build Tools folder, for
  example `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`, and **Architecture** to
  `amd64`. Leave the debugger on Bundled LLDB.
- **MinGW:** **+ → MinGW**. Set **Toolset** to `C:\Qt\Tools\mingw1310_64`, not CLion's bundled MinGW.
  Leave the debugger on Bundled GDB.

Wait until CLion shows the C and C++ compilers it detected. CMake and the build tool can stay on
"Bundled".

The program and the tests need Qt's DLLs. The easiest way is to put Qt's `bin` folder on `PATH` for
everything that runs with this toolchain. To do that, create a script **outside the repository**,
for example `%USERPROFILE%\aion-qt-env.bat`:

```bat
set PATH=C:\Qt\6.12.0\msvc2022_64\bin;%PATH%
```

Then choose it in the toolchain under **Add environment → From file**.

### 2. CMake profile

Under **CMake**, set up the **Debug** profile. Optionally add a **Release** profile with the same
settings.

- **Toolchain:** the one from step 1. With "Default", CLion may pick a compiler that doesn't match
  your Qt kit.
- **Generator:** Ninja
- **CMake options:** `-DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64`

After **OK**, the CMake tool window should name the compiler you chose, for example
`The CXX compiler identification is MSVC ...` or `... is GNU 13.1.0`. If you change the toolchain
later, also run **Tools → CMake → Reset Cache and Reload Project**. The build folder otherwise keeps
the old compiler.

Build with **Build → Build Project** (Ctrl+F9).

### 3. Running and debugging Aion

Choose **Aion** in the run configuration dropdown, then Run or Debug.

If you didn't set up the environment file in step 1, add Qt's `bin` folder to the run configuration:
open **Run → Edit Configurations → Aion → Environment variables** and add
`PATH=C:\Qt\6.12.0\msvc2022_64\bin;$PATH$`. Without it, Aion stops right away because Windows can't
find `Qt6Core.dll` or `Qt6Cored.dll`.

To see `QString`, `QList` and other Qt types in the debugger, go to
**Debugger → Data Views → C/C++** and turn on **Enable NatVis renderers for LLDB** (MSVC). If Qt
types still show only their internal fields, add a `qt6.natvis` file. It comes with the
[Qt VS Tools](https://doc.qt.io/qtvstools/).

### 4. Tests

Every `tests/test_*.cpp` is its own test program, registered with CTest
(see [tests/CMakeLists.txt](tests/CMakeLists.txt)). A new test file is picked up the next time CMake
runs, so it doesn't have to be listed anywhere.

1. In **Run → Edit Configurations**, use the **All CTest** configuration. If it's missing, create it
   with **+ → CTest Application**.
2. Add `QT_QPA_PLATFORM=offscreen` to its **Environment variables**, so widget tests don't open
   windows. Without the environment file from step 1, also add the `PATH` entry from step 3.
3. Run it. The results show in the test runner, where you can rerun single tests.

To give new run configurations these variables automatically, set them in
**Edit configuration templates…** under **CTest Application**. Don't add `QT_QPA_PLATFORM=offscreen`
to the **CMake Application** template, or Aion's window won't appear.

From a terminal:

```
ctest --test-dir cmake-build-debug --output-on-failure
```

### Files that stay out of git

`.idea/` is in `.gitignore`. CLion's `cmake-build-*` folders each get their own `.gitignore` that
ignores the whole folder. Don't commit either of them.

## Qt Creator

Open `CMakeLists.txt` and choose a kit. The kit sets the matching compiler and puts Qt on `PATH`, so
nothing else is needed. To run the tests, use **Tools → Tests → Run All Tests**, or run `ctest` in
the build folder.

## Command line (Windows, MSVC)

[scripts/build.ps1](scripts/build.ps1) configures, builds and tests with Visual Studio 2022 or its
Build Tools, in the `build-vs` folder, using all cores:

```powershell
.\scripts\build.ps1 test run     # build, test, and start Aion if the tests pass
```

It finds the newest `msvc*_64` Qt kit under `C:\Qt` or `D:\Qt`. For a kit elsewhere, pass
`-QtDir C:\path\to\6.12.0\msvc2022_64` once or set `AION_QT_DIR`. The tests run off screen, so their
windows don't appear. `Get-Help .\scripts\build.ps1 -Examples` lists the other actions.

To run one test program without ctest, put the kit's `bin` first in `PATH` and set the same variables:

```powershell
$env:Path = "D:\Qt\6.12.0\msvc2022_64\bin;$env:Path"
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_FORCE_STDERR_LOGGING = '1'
.\build-vs\Debug\test_account.exe
```

- Without Qt's `bin` in `PATH` the program stops at once with exit code `-1073741515` (0xC0000135,
  a DLL not found) and prints nothing. Other programs on `PATH` (KeePassXC, MiKTeX) bring their own
  `Qt6Core.dll`, which a Release build may pick up instead of the kit's.
- Without a console window (when an IDE, a script or an agent starts the program with pipes), Qt Test
  sends its results to the debugger output (`OutputDebugString`) instead of stdout.
  `QT_FORCE_STDERR_LOGGING=1` brings them back, or write them to a file with `-o result.txt,txt`.
- A test program's exit code is the number of failed tests, so `53` means 53 failures, not a crash.

## Translations

Aion's texts are written in English (UK) in the code, and translated in `translations/aion_<code>.ts`,
one file per language, which the build turns into the program's translations. A text that isn't
translated yet shows in English, so a language can be translated bit by bit. The badges in the README
show how far each one is, and CI keeps them current
([.forgejo/workflows/translations.yml](.forgejo/workflows/translations.yml)).

Building the translations needs Qt's Linguist tools (`lupdate`, `lrelease`). The Qt installer's kits
have them; on Linux they're packages of their own (`qt6-qttools-devel` on Fedora,
`qt6-tools-dev` and `qt6-l10n-tools` on Debian and Ubuntu). Without them CMake warns, and Aion is
built in English only.

### Texts in the code

- Mark every text the user sees: `tr("…")` in a class with `Q_OBJECT`, `ClassName::tr("…")` in a
  helper next to it, and `QCoreApplication::translate("Context", "…")` elsewhere. Texts only for the
  log, the command line or the technical information in About stay English and unmarked.
- Write whole sentences with placeholders, `tr("%1 of %2 spent")`, instead of putting a sentence
  together from pieces: other languages need other word orders.
- Counts: `AppLocale::count(n, tr("%1 transaction"), tr("%1 transactions"))`, which also formats the
  number for the language.
- Dates, month names and file sizes: `AppLocale` (`src/applocale.h`), never `QDate::toString` with a
  fixed pattern, so they follow the language.
- Names Aion relies on (the built-in categories, keys in the settings and the database) stay English
  in the data and are translated only where they're shown, like `displayCategoryName()`.

### After adding or changing texts

```
cmake --build <build folder> --target update_translations
```

adds new and changed texts to every `.ts` file and drops the ones no longer in the code. Translate
them with Qt Linguist (`linguist translations/aion_de.ts`, in the kit's `bin` folder) or in a text
editor, then commit the `.ts` files with the change. Untranslated texts are fine, but a translation
must keep its text's placeholders (`%1`, `%2`, …) and HTML tags; `test_language` checks that.

### Adding a language

1. Add its code to `AION_LANGUAGES` in [CMakeLists.txt](CMakeLists.txt) and to `Language::available()`
   in [src/language.cpp](src/language.cpp), with its name in that language (`{"fr", "Français"}`).
2. Create `translations/aion_<code>.ts` with only this in it, and run `update_translations`:

   ```xml
   <?xml version="1.0" encoding="utf-8"?>
   <!DOCTYPE TS>
   <TS version="2.1" language="fr_FR" sourcelanguage="en_GB">
   </TS>
   ```

3. Give it its date patterns in `pattern()` and its locale in `AppLocale::locale()`
   ([src/applocale.cpp](src/applocale.cpp)); without them, dates are written the English way.

## Database changes

The schema comes from the migrations in [db/migrations](db/migrations), `<number>_<name>.sql`,
applied in order. Opening a database applies the ones it hasn't had yet, after a backup.

- Change the schema with a new migration, numbered one above the newest. A released migration never
  changes: users' databases already ran it, so a change would never reach them.
  `releasedMigrationsNeverChange` in [tests/test_migrationmanager.cpp](tests/test_migrationmanager.cpp)
  checks that by their checksums.
- A migration that changes the data the user already has, not only the schema, gets a test of what it
  does to such data. Start it from a database at the version before (`TestDbFixture::createAtVersion`)
  and fill that in the way the older Aion would have. `upgradingFromEveryVersionKeepsTheData` already
  checks that every version upgrades to the same schema as a new database and keeps its data.

When a release has new migrations, add each to `releasedMigrationsNeverChange` with its SHA-256,
taken from the file as git stores it, with Unix line endings (Git Bash, after committing it):

```bash
git show HEAD:db/migrations/002_starter_names.sql | sha256sum
```

## Before you send a change

- Build without new warnings. To check, configure with `-DAION_WARNINGS_AS_ERRORS=ON`. MSVC and GCC
  warn about different things, and CI builds with GCC.
- Run the whole test suite.
- If you add code to `src/`, add or update tests in `tests/`.
- If you add or change texts the user sees, mark them for translation and run `update_translations`
  (see [Translations](#translations)).
- If you change the database, add a migration rather than changing one
  (see [Database changes](#database-changes)).
- A line that can never run, like the `return` after a `switch` that handles every value, can end
  with `// GCOVR_EXCL_LINE: <why>`, and the coverage report leaves it out. Use it only for lines that
  really can't run, not for ones that are just hard to test, like a failing disk.

## Builds and releases

CI runs on Codeberg ([.forgejo/workflows/build.yml](.forgejo/workflows/build.yml)), on a
self-hosted runner with four build images:

- `linux-qt`: Fedora with Qt for Linux, Qt for MinGW and Wine
  ([ci/images/fedora44.Dockerfile](ci/images/fedora44.Dockerfile)). Builds and tests Linux and
  Windows.
- `linux-appimage`: Ubuntu 22.04 with the official Qt and linuxdeploy
  ([ci/images/ubuntu22.04.Dockerfile](ci/images/ubuntu22.04.Dockerfile)). Builds, tests and packs the
  AppImage, on an older system so it runs on older distributions too.
- `linux-debian`: Debian 13 with Debian's Qt
  ([ci/images/debian13.Dockerfile](ci/images/debian13.Dockerfile)). Builds, tests and packs the DEB.
- `linux-qt-win7`: the `linux-qt` image plus Qt 6.8.5 for MinGW with the
  [qt6windows7](https://github.com/crystalidea/qt6windows7) patches
  ([ci/images/fedora44-win7.Dockerfile](ci/images/fedora44-win7.Dockerfile)). Builds, tests and
  packs the Windows 7/8 package. It starts from `linux-qt`, so rebuild it whenever that one changes.

- **Every push and pull request to `develop`:** Linux is built and tested on Fedora, and Windows
  is cross-built with MinGW and tested under Wine. A newer push cancels the run of an older commit.
  Changes to documentation alone (Markdown files, `docs/`) don't start a run.
- **Pull requests, and pushes that change the build** (a `CMakeLists.txt`, the DEB or AppImage
  script or image, or the workflow), are also built and tested on Debian and for the AppImage.
  These test the older Qt versions, so a function only newer Qt has shows up in the nightly run
  at the latest.
- **Publishing runs**, nightly (03:00 UTC, only when more than documentation changed on `develop`
  since the last `staging` build) and when started by hand with **Run workflow** on the Actions page, also:
  - pack the Linux build as an RPM for that Fedora release ([scripts/package-rpm.sh](scripts/package-rpm.sh)),
    using Fedora's Qt, and check that it installs, starts and uninstalls;
  - do the same with a DEB for that Debian release ([scripts/package-deb.sh](scripts/package-deb.sh)),
    using Debian's Qt;
  - package Windows as a zip ([scripts/package-windows.sh](scripts/package-windows.sh)) and check
    that it starts with only its own files;
  - build the installer ([installer/aion.nsi](installer/aion.nsi), NSIS), and check that it installs,
    starts and uninstalls under Wine;
  - pack the AppImage ([scripts/package-appimage.sh](scripts/package-appimage.sh)) and check that it
    starts;
  - build Linux once more with `-DAION_SANITIZERS=ON` and run the tests with AddressSanitizer and
    UndefinedBehaviorSanitizer, which stop a test at a memory error or undefined behaviour that a
    normal run passes by luck. Leaks aren't checked yet. A failure there fails the run but doesn't
    hold up the release. To run it yourself (GCC or Clang, Linux or WSL):
    `cmake -S . -B build-sanitizers -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAION_SANITIZERS=ON`, build,
    then `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitizers --output-on-failure`;
  - nightly, with the draft, or when its box is ticked: build the Windows 7/8 package and its
    installer ([installer/aion-win7.nsi](installer/aion-win7.nsi)) the same way, and check that
    every Windows function it calls exists on Windows 7 SP1
    ([scripts/check-win7-imports.sh](scripts/check-win7-imports.sh), with the lists in
    [ci/win7-exports](ci/win7-exports/README.md)). Not first-class support: a failure there only
    warns, and the release then goes out without these files;
  - update the releases ([scripts/publish-release.sh](scripts/publish-release.sh)): always the
    `staging` pre-release, the version's draft only when started by hand with the box ticked.
- **Coverage**, nightly (04:00 UTC, [.forgejo/workflows/coverage.yml](.forgejo/workflows/coverage.yml),
  only when more than documentation changed since the measured commit): builds with
  `-DAION_COVERAGE=ON`, runs [scripts/coverage.sh](scripts/coverage.sh) and commits `COVERAGE.md`
  and the badge to `develop` as "Aion CI" when the report changed. That commit doesn't start a build.

Packaging only in these runs saves the runner and Codeberg from packing and uploading every push.

If Aion starts using a Qt module it didn't use before, add it to the plugin table in
`package-windows.sh`. The packaging stops and says so until you do.

### Build images

The runner for this repo (Forgejo Runner with Docker Compose, in `/srv/forgejo-runner` on the VPS) runs jobs 
in a docker-in-docker container, `docker_dind`. Images are built and kept inside it, in the volume
`dind-data`, so `docker images` on the host doesn't show them. Its Docker listens on TCP only, hence
the `-H tcp://localhost:2375` in the commands below.

To add or update an image, on your VPS:

1. Copy the Dockerfile from `ci/images/` to `/srv/forgejo-runner/`, for example with `nano`.
2. Build it inside `docker_dind`. This adds the image and leaves the others alone; building with
   the same name again replaces it.
   ```
   cd /srv/forgejo-runner && docker exec -i docker_dind docker -H tcp://localhost:2375 build -t local/qt-builder:ubuntu22.04 - < ubuntu22.04.Dockerfile
   ```
3. Check that it's there:
   ```
   docker exec docker_dind docker -H tcp://localhost:2375 images
   ```

A **new** image also needs a label, so workflows can name it in `runs-on`:

4. Back up the runner's config:
   ```
   cp -p data/runner-config.yml data/runner-config.yml.bak
   ```
5. Add the label under `labels:` in `data/runner-config.yml`, for example
   `- linux-appimage:docker://local/qt-builder:ubuntu22.04`, indented like the others.
6. Restart only the runner (not `docker_dind`, which holds the images), and check its log:
   ```
   docker compose restart runner
   docker logs --tail 20 runner
   ```
   The runner's page on Codeberg then lists the new label.

To undo: restore `data/runner-config.yml.bak` and restart the runner, and remove an image with
`docker exec docker_dind docker -H tcp://localhost:2375 rmi <name>`. Don't share
`data/runner-config.yml` as a file: it can contain the runner's token.

### Releases

- **Development build:** the pre-release `staging` has the latest build of `develop`, at most a day
  old. For a newer one, start **Run workflow** on the Actions page.
- **Versions:** the version in `CMakeLists.txt` (`project(Aion VERSION …)`) has a **draft** release
  with the tag `v<version>`. Until it's published, every run started by hand with **Also prepare
  the draft release** ticked moves the tag to the new commit and replaces the files. Notes written
  in the draft are saved.

To release:

1. If the release has new migrations, add their checksums to the test
   (see [Database changes](#database-changes)) and commit that to `develop`.
2. On Codeberg, open **Actions**, choose the workflow, click **Run workflow** on `develop` with
   **Also prepare the draft release** ticked, and wait for it to pass. The draft then has that
   commit's build. If the Windows 7/8 job failed, the draft has no Windows 7/8 files: check its
   log, and decide whether to fix it first or release without them.
3. Open the draft under **Releases**, write the release notes, and click **Publish**.
   The release and its tag stay as they are from then on.
4. Raise the version in `CMakeLists.txt` for the next release. Its draft is made the next time you
   prepare one. Raising it early is fine, since a draft is only published by hand.

Note: Releases use the run's own token. If that may not create tags and releases, add a Codeberg access
token with the permission "write: repository" as the repository secret `RELEASE_TOKEN` to your fork.
