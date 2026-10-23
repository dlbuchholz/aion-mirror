# Build image for the Linux AppImage (runner label linux-appimage).
#
# An AppImage only runs on systems at least as new as the one it was built on (glibc), so this
# uses Ubuntu 22.04, older than most systems people run today. Its own Qt packages are too old for
# Aion, so the official Qt is installed with aqtinstall. linuxdeploy makes the AppImage.
#
# Build it inside the runner's docker-in-docker container (see CONTRIBUTING.md):
#   docker exec -i docker_dind docker build -t local/qt-builder:ubuntu22.04 - < ubuntu22.04.Dockerfile
FROM ubuntu:22.04

ARG QT_VERSION=6.12.0
ARG NODE_VERSION=22.12.0
ARG CMAKE_VERSION=3.31.6

ENV DEBIAN_FRONTEND=noninteractive

# Compiler and build tools; the libraries Qt's xcb platform plugin needs (linuxdeploy looks them up
# while bundling); python for aqtinstall
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential ninja-build git ca-certificates curl xz-utils file \
        python3 python3-pip \
        libgl1-mesa-dev libegl1 libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 \
        libxkbcommon0 libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 \
        libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 \
        libxcb-xkb1 libxcb-xfixes0 libxcb-sync1 libxcb-shm0 libxcb-util1 libx11-xcb1 \
    && rm -rf /var/lib/apt/lists/*

# CMake from pip: Qt 6.12 needs CMake 3.25 or newer, Ubuntu 22.04 has 3.22
RUN pip3 install --no-cache-dir "cmake==$CMAKE_VERSION" && cmake --version

# The official Qt (open source, LGPL), with Qt SVG. Only the SQLite driver stays: the others need
# database client libraries that aren't installed, and linuxdeploy would fail looking for them.
# Their CMake files go too, or CMake stops because the files they list are missing.
RUN pip3 install --no-cache-dir aqtinstall \
    && aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 -O /opt/qt \
    && qt="/opt/qt/$QT_VERSION/gcc_64" \
    && test -d "$qt/lib/cmake/Qt6Svg" \
    && find "$qt/plugins/sqldrivers" -name '*.so' ! -name 'libqsqlite.so' -delete \
    && find "$qt/lib/cmake/Qt6Sql" -name 'Qt6Q*DriverPlugin*.cmake' ! -name 'Qt6QSQLiteDriverPlugin*' -delete \
    && rm -rf /root/.cache aqtinstall.log

# Node for the CI actions (checkout, artifacts); Ubuntu 22.04's own Node is too old for them
RUN curl -fsSL "https://nodejs.org/dist/v$NODE_VERSION/node-v$NODE_VERSION-linux-x64.tar.xz" \
        | tar -xJ -C /opt \
    && ln -s "/opt/node-v$NODE_VERSION-linux-x64/bin/node" /usr/local/bin/node

# linuxdeploy with its Qt plugin. They are AppImages themselves; containers have no FUSE, so they
# unpack themselves on each start instead (APPIMAGE_EXTRACT_AND_RUN).
RUN mkdir -p /opt/linuxdeploy && cd /opt/linuxdeploy \
    && curl -fsSLO https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && curl -fsSLO https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
    && chmod +x *.AppImage

ENV PATH=/opt/qt/${QT_VERSION}/gcc_64/bin:/opt/linuxdeploy:${PATH} \
    CMAKE_PREFIX_PATH=/opt/qt/${QT_VERSION}/gcc_64 \
    QMAKE=/opt/qt/${QT_VERSION}/gcc_64/bin/qmake \
    APPIMAGE_EXTRACT_AND_RUN=1 \
    QT_QPA_PLATFORM=offscreen
