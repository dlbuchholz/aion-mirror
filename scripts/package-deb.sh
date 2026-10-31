#!/usr/bin/env bash
# Packs a Linux build of Aion into a DEB that uses the distribution's Qt, so it's for the Debian
# release it was built on.
#
#   scripts/package-deb.sh <build-dir> <output-dir>
#
# Runs in the Debian build image (ci/images/debian13.Dockerfile), on a build configured with
# -DAION_USE_SYSTEM_QT=ON. Prints the DEB's path.
set -euo pipefail

build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
source_dir=$(realpath "$(dirname "$0")/..")

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_dir/CMakeCache.txt")
commit=$(git -C "$source_dir" rev-parse --short HEAD)
debian=$(. /etc/os-release && echo "$VERSION_ID")
name="aion-$version-$commit-debian$debian-amd64"

mkdir -p "$output_dir"
(cd "$build_dir" && cpack -G DEB -B "$output_dir/deb" -D CPACK_PACKAGE_FILE_NAME="$name") >&2
mv "$output_dir/deb/$name.deb" "$output_dir/"
rm -rf "$output_dir/deb"
echo "$output_dir/$name.deb"
