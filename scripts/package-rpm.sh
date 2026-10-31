#!/usr/bin/env bash
# Packs a Linux build of Aion into an RPM that uses the distribution's Qt, so it's for the Fedora
# release it was built on.
#
#   scripts/package-rpm.sh <build-dir> <output-dir>
#
# Runs in the Fedora build image (ci/images/fedora44.Dockerfile), on a build configured with
# -DAION_USE_SYSTEM_QT=ON. Prints the RPM's path.
set -euo pipefail

build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
source_dir=$(realpath "$(dirname "$0")/..")

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_dir/CMakeCache.txt")
commit=$(git -C "$source_dir" rev-parse --short HEAD)
fedora=$(. /etc/os-release && echo "$VERSION_ID")
name="aion-$version-$commit-fedora$fedora-x86_64"

mkdir -p "$output_dir"
(cd "$build_dir" && cpack -G RPM -B "$output_dir/rpm" -D CPACK_PACKAGE_FILE_NAME="$name") >&2
mv "$output_dir/rpm/$name.rpm" "$output_dir/"
rm -rf "$output_dir/rpm"
echo "$output_dir/$name.rpm"
