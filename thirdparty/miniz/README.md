# miniz

Reads the zip archives that `.xlsx` and `.ods` spreadsheets are (see `src/spreadsheet.cpp`).

- Version 3.0.2, unchanged: `miniz.c`, `miniz.h` and `LICENSE` from
  https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
  (SHA-256 of the zip: `ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5`)
- License: MIT, see `LICENSE`.
- Built as its own library in `CMakeLists.txt`, without Aion's warnings, and with
  `MINIZ_NO_ZLIB_COMPATIBLE_NAMES` so it doesn't clash with the zlib Qt uses.

To update, replace the three files with those of a newer release and update this file.
