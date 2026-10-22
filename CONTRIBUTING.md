# Contributing to Aion

This page explains how to set up a development environment. For a quick build from the command line,
see [Building](README.md#building) in the README.

## What you need

- Qt 6.5 or newer, with the **Svg** module, from the [Qt installer](https://www.qt.io/download-qt-installer)
- CMake 3.19 or newer and Ninja. CLion and Qt Creator bundle both, and the Qt installer has them under
  `Qt\Tools`.
- A C++ compiler that **matches your Qt kit** (see below)

## Choosing a compiler (Windows)

Aion builds with MSVC and with MinGW. CI builds Linux and a MinGW build for Windows, and the coverage
script needs MSVC. Use whichever you like. The one rule is that the compiler has to match the Qt kit
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

## Before you send a change

- Build without new warnings. To check, configure with `-DAION_WARNINGS_AS_ERRORS=ON`. MSVC and GCC
  warn about different things, and CI builds with GCC.
- Run the whole test suite.
- If you add code to `src/`, add or update tests in `tests/`.
