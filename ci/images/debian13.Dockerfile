# Build image for the Debian package (runner label linux-debian).
#
# Build it inside the runner's docker-in-docker container (see CONTRIBUTING.md):
#   docker exec -i docker_dind docker -H tcp://localhost:2375 build -t local/qt-builder:debian13 - < debian13.Dockerfile
#
# Uses Debian's own Qt, which the package then depends on. Pin the Debian release: a package is
# for the release it was built on.
FROM debian:13

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        git nodejs ca-certificates \
        cmake ninja-build build-essential \
        # reuses compiler results from earlier CI runs (see the workflow)
        ccache \
        qt6-base-dev qt6-svg-dev \
        # the plugins Qt loads at runtime; nothing else in this list brings them
        libqt6sql6-sqlite qt6-svg-plugins \
        # the package (scripts/package-deb.sh)
        dpkg-dev file \
    && rm -rf /var/lib/apt/lists/*

ENV QT_QPA_PLATFORM=offscreen
