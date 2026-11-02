# Build image for the Windows 7/8 package (runner label linux-qt-win7). Not first-class support:
# the regular Windows package (Windows 10/11) comes from fedora44.Dockerfile and isn't affected.
#
# Build fedora44.Dockerfile first: this image starts from it, so it has the same MinGW compiler,
# Wine and NSIS. Rebuild this one whenever that one changes. Inside the runner's docker-in-docker
# container (see CONTRIBUTING.md):
#   docker exec -i docker_dind docker -H tcp://localhost:2375 build -t local/qt-builder:fedora44-win7 - < fedora44-win7.Dockerfile
#
# Qt 6.9 and newer don't run on Windows 7/8, so this builds Qt 6.8 with the patches from
# qt6windows7 (https://github.com/crystalidea/qt6windows7), which replace the Windows 8+ functions
# Qt calls. Only qtbase is patched; qtsvg runs unchanged. Two builds of Qt, done once here:
#   - /opt/qt-host: Qt for Linux, whose moc/rcc/uic run during the cross build. It must be the
#     same version as the Windows Qt, so Fedora's own Qt can't be used.
#   - /opt/qt-win7: Qt for Windows, cross-compiled with Fedora's MinGW. That links msvcrt.dll,
#     which every Windows version has, so Windows 7 needs no Visual C++ or UCRT update.
# Qt brings its own zlib, libpng, libjpeg, PCRE2, HarfBuzz, FreeType and SQLite, so the package
# needs no Fedora DLLs besides the MinGW runtime.

# The build stage: sources, build folders and all. Docker keeps each finished step, so a failed
# step can be fixed and the build continues from there.
FROM local/qt-builder:fedora44 AS qt

ARG QT_VERSION=6.8.5
ARG QTBASE_SHA256=fc95010e94d8b1ec342eda9281fd278d5f096cf9ad8f1a0adfba9575438b0abc
ARG QTSVG_SHA256=8ef4841a8b81d7192d908ea344137cf66a29cc110382588d91c62508e0e15e3a
# qt6windows7's tag v6.8.5
ARG QT6WINDOWS7_COMMIT=b2f860af8497f5e9f5afc0d67cea598ebde27d78

RUN dnf -y install --setopt=install_weak_deps=False curl xz && dnf clean all

# The Qt sources with their checksums, and the patched files copied over qtbase's
WORKDIR /src
RUN set -eu \
    && url="https://download.qt.io/archive/qt/6.8/$QT_VERSION/submodules" \
    && curl -fsSLO "$url/qtbase-everywhere-opensource-src-$QT_VERSION.tar.xz" \
    && curl -fsSLO "$url/qtsvg-everywhere-opensource-src-$QT_VERSION.tar.xz" \
    && echo "$QTBASE_SHA256  qtbase-everywhere-opensource-src-$QT_VERSION.tar.xz" | sha256sum -c - \
    && echo "$QTSVG_SHA256  qtsvg-everywhere-opensource-src-$QT_VERSION.tar.xz" | sha256sum -c - \
    && tar -xJf "qtbase-everywhere-opensource-src-$QT_VERSION.tar.xz" \
    && tar -xJf "qtsvg-everywhere-opensource-src-$QT_VERSION.tar.xz" \
    && mv "qtbase-everywhere-src-$QT_VERSION" qtbase \
    && mv "qtsvg-everywhere-src-$QT_VERSION" qtsvg \
    && rm ./*.tar.xz \
    && git init -q qt6windows7 \
    && git -C qt6windows7 fetch -q --depth 1 https://github.com/crystalidea/qt6windows7.git "$QT6WINDOWS7_COMMIT" \
    && git -C qt6windows7 checkout -q FETCH_HEAD \
    && cp -r qt6windows7/qtbase/. qtbase/

# Qt for Linux, only for its build tools. The patched files are Windows-only, so they don't
# change this build.
RUN mkdir /build-host && cd /build-host \
    && /src/qtbase/configure -prefix /opt/qt-host -release -nomake examples -nomake tests -no-opengl \
    && cmake --build . --parallel \
    && cmake --install .

# Qt for Windows, cross-compiled with Fedora's MinGW toolchain. No OpenGL: Aion draws with
# Qt Widgets only, and Windows 7 PCs often have no working OpenGL driver. Only the SQLite driver:
# the others need database client libraries.
# Qt Core links MinGW's synchronization library ahead of kernel32, for WaitOnAddress, which the
# patches no longer use. But that library lists older functions too (Sleep, ...), so Qt Core would
# take them from api-ms-win-core-synch-l1-2-0.dll, which Windows 7 doesn't have, instead of from
# kernel32.dll. So the library goes; grep fails if a new Qt version changes that line.
RUN grep -q '^ *synchronization$' /src/qtbase/src/corelib/CMakeLists.txt \
    && sed -i '/^ *synchronization$/d' /src/qtbase/src/corelib/CMakeLists.txt \
    && mkdir /build-win7 && cd /build-win7 \
    && /src/qtbase/configure -prefix /opt/qt-win7 -release -shared \
        -xplatform win32-g++ -qt-host-path /opt/qt-host \
        -nomake examples -nomake tests \
        -no-opengl -no-dbus -no-icu -no-openssl -schannel \
        -qt-zlib -qt-libpng -qt-libjpeg -qt-pcre -qt-harfbuzz -qt-freetype \
        -qt-sqlite -no-sql-odbc -no-sql-psql -no-sql-mysql -no-sql-ibase -no-sql-oci -no-sql-db2 -no-sql-mimer \
        -- -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw64.cmake \
    && cmake --build . --parallel \
    && cmake --install .

# Qt SVG for Windows, configured through the Windows Qt (qt-cmake sets up the cross build)
RUN /opt/qt-win7/bin/qt-cmake -S /src/qtsvg -B /build-svg -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build-svg --parallel \
    && cmake --install /build-svg

# Where this Qt comes from, for the package (scripts/package-windows.sh copies it as QT-SOURCE.txt).
# The LGPL asks for the source of a changed Qt to be named.
RUN url="https://download.qt.io/archive/qt/6.8/$QT_VERSION/submodules" \
    && printf '%s\n' \
        "Qt $QT_VERSION (Qt Base and Qt SVG), built for Aion's Windows 7/8 package. Qt is used under" \
        "the LGPL version 3 (LICENSE-LGPL.txt)." \
        "" \
        "Built from Qt's source archives" \
        "  $url/qtbase-everywhere-opensource-src-$QT_VERSION.tar.xz" \
        "  $url/qtsvg-everywhere-opensource-src-$QT_VERSION.tar.xz" \
        "with these changes to Qt Base:" \
        "  - the files in the qtbase folder of qt6windows7 (https://github.com/crystalidea/qt6windows7)," \
        "    commit $QT6WINDOWS7_COMMIT, copied over it" \
        "  - Qt Core doesn't link the synchronization library (the line in src/corelib/CMakeLists.txt" \
        "    removed)" \
        "" \
        "How: ci/images/fedora44-win7.Dockerfile in Aion's source (https://codeberg.org/spectre_0/aion)." \
        > /opt/qt-win7/SOURCE.txt

# The DLLs each Qt DLL and plugin needs, for the build log. Anything besides Windows' own DLLs and
# the MinGW runtime (libstdc++, libgcc, libwinpthread) came from Fedora by mistake. Fails on an
# api-ms-win-* import: Windows 7 lacks most of those, and Qt needs none of them.
RUN find /opt/qt-win7 -iname '*.dll' | sort | while read -r dll; do \
        echo "${dll#/opt/qt-win7/}: $(x86_64-w64-mingw32-objdump -p "$dll" | sed -n 's/^\s*DLL Name: //p' | sort -f | tr '\n' ' ')"; \
    done | tee /tmp/imports.txt \
    && if grep -i 'api-ms-win-' /tmp/imports.txt; then echo "Imports DLLs Windows 7 doesn't have" >&2; exit 1; fi

# The image itself: the base image plus the two Qt builds, without sources and build folders
FROM local/qt-builder:fedora44

COPY --from=qt /opt/qt-host /opt/qt-host
COPY --from=qt /opt/qt-win7 /opt/qt-win7

# Tests under Wine load Qt from here, not Fedora's newer Qt DLLs in the MinGW folder (Wine
# separates paths with ;). The MinGW folder is still needed for its runtime DLLs.
ENV QT_WIN7_ROOT=/opt/qt-win7 \
    QT_HOST_PATH=/opt/qt-host \
    WINEPATH="/opt/qt-win7/bin;/usr/x86_64-w64-mingw32/sys-root/mingw/bin" \
    QT_PLUGIN_PATH=/opt/qt-win7/plugins
