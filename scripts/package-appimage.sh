#!/usr/bin/env bash
# Packs a Linux build of Aion into an AppImage: one file that runs on most distributions.
#
#   scripts/package-appimage.sh <build-dir> <output-dir>
#
# Runs in the Ubuntu 22.04 build image (ci/images/ubuntu22.04.Dockerfile). linuxdeploy copies Aion,
# its Qt libraries and plugins (through its Qt plugin) and the other libraries it needs into an
# AppDir; then the licenses are added and the AppDir is packed. Prints the AppImage's path.
set -euo pipefail

build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
source_dir=$(realpath "$(dirname "$0")/..")
qt_dir=${CMAKE_PREFIX_PATH:?CMAKE_PREFIX_PATH must point at Qt}

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_dir/CMakeCache.txt")
commit=$(git -C "$source_dir" rev-parse --short HEAD)
name="aion-$version-$commit-linux-x86_64"
appdir="$build_dir/AppDir"
rm -rf "$appdir"
mkdir -p "$output_dir"

# The icon's file name has to match Icon= in the desktop file
cp "$source_dir/assets/aion-logo.svg" "$build_dir/aion.svg"

# 1. Fill the AppDir. The offscreen platform plugin comes along, so Aion also starts without a
#    screen (CI checks the AppImage that way).
EXTRA_PLATFORM_PLUGINS=libqoffscreen.so linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$build_dir/Aion" \
    --desktop-file "$source_dir/packaging/linux/aion.desktop" \
    --icon-file "$build_dir/aion.svg" \
    --plugin qt >&2                                  # Its log on stderr; stdout is just the path

# 2. Licenses: Aion's own, what's built into Aion, and where each bundled library comes from
doc="$appdir/usr/share/doc/aion"
mkdir -p "$doc"
cp "$source_dir/LICENSE" "$doc/LICENSE"
cp "$source_dir/assets/icons/LICENSE" "$doc/LICENSE-icons"
cp "$source_dir/thirdparty/miniz/LICENSE" "$doc/LICENSE-miniz"
{
    echo "Aion contains interface icons based on Tabler Icons (https://tabler.io/icons),"
    echo "under the MIT license in LICENSE-icons."
    echo
    echo "Aion contains miniz (https://github.com/richgel999/miniz), under the MIT license"
    echo "in LICENSE-miniz."
    echo
    echo "Aion bundles these libraries. Qt's are the official build, used under the LGPL version 3"
    echo "(source: https://download.qt.io). The others come from the Ubuntu 22.04 package named next"
    echo "to them; source and license: https://packages.ubuntu.com/jammy/<package>."
    echo
    for lib in "$appdir"/usr/lib/*.so* "$appdir"/usr/plugins/*/*.so; do
        [[ -e $lib ]] || continue
        relative=${lib#"$appdir/usr/"}
        base=$(basename "$lib")
        if [[ -e $qt_dir/lib/$base || $relative == plugins/* ]]; then
            echo "$relative: Qt $(basename "$(dirname "$qt_dir")")"
        else
            package=$(dpkg -S "*/$base" 2>/dev/null | head -n 1 | cut -d: -f1)
            echo "$relative: ${package:-unknown package}"
        fi
    done
} > "$doc/THIRD-PARTY"

# 3. Pack it
appimage="$output_dir/$name.AppImage"
rm -f "$appimage"
OUTPUT="$appimage" linuxdeploy-x86_64.AppImage --appdir "$appdir" --output appimage >&2
echo "$appimage"
