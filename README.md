# Aion

[![Coverage](docs/coverage-badge.svg)](COVERAGE.md)

A personal finance app for the desktop.
Built with Qt 6 Widgets and SQLite.

- [Test coverage](COVERAGE.md): which files are covered by tests
- [Roadmap](ROADMAP.md): what's planned

## Building

Needs Qt 6.5 or newer and CMake 3.19 or newer. Open `CMakeLists.txt` in Qt Creator, or:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=<path to Qt>
cmake --build build
ctest --test-dir build
```

To update the coverage report (Windows, MSVC debug build, [OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage)):

```
.\scripts\coverage.ps1 -BuildDir <build folder> -QtBin <Qt bin folder> -Publish
```
