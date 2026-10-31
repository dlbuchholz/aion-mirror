# Build image for Linux and Windows (runner label linux-qt).
#
# Build it inside the runner's docker-in-docker container (see CONTRIBUTING.md):
#   docker exec -i docker_dind docker -H tcp://localhost:2375 build -t local/qt-builder:fedora44 - < fedora44.Dockerfile
#
# One build image for both targets:
#   - native Linux build  (Fedora's Qt 6)
#   - Windows cross build (Fedora's MinGW Qt 6, same version), tests run under Wine
# Pin the Fedora release – new releases occasionally ship broken mingw-qt6 packages.
FROM fedora:44

RUN dnf -y install --setopt=install_weak_deps=False \
        git nodejs which findutils \
        cmake ninja-build gcc-c++ \
        # reuses compiler results from earlier CI runs (see the workflow)
        ccache \
        # Linux Qt (also provides the host tools moc/rcc/uic for the cross build)
        qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel qt6-qtsvg-devel \
        # Windows cross toolchain + Qt
        mingw64-gcc-c++ \
        mingw64-qt6-qtbase mingw64-qt6-qtdeclarative mingw64-qt6-qttools mingw64-qt6-qtsvg \
        # running Windows unit tests
        wine xorg-x11-server-Xvfb xorg-x11-xauth \
        # the Windows installer (installer/aion.nsi)
        mingw32-nsis \
        # the RPM (scripts/package-rpm.sh)
        rpm-build \
        # test coverage (scripts/coverage.sh, nightly)
        gcovr \
    && dnf clean all
    # more modules: add e.g. qt6-qtcharts-devel + mingw64-qt6-qtcharts

ENV QT_QPA_PLATFORM=offscreen \
    MINGW_ROOT=/usr/x86_64-w64-mingw32/sys-root/mingw \
    WINEPREFIX=/opt/wine \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES="mscoree,mshtml="

# Qt DLLs + plugins for test executables under Wine
ENV WINEPATH=${MINGW_ROOT}/bin \
    QT_PLUGIN_PATH=${MINGW_ROOT}/lib/qt6/plugins

# Initialise the Wine prefix once at build time instead of in every job
RUN wineboot --init && wineserver -w
