#!/usr/bin/env bash
# Packs a MinGW cross-build of Liora into a zip that runs on any Windows PC.
#
#   scripts/package-windows.sh <build-dir> <output-dir>
#
# Runs in the Fedora build image (see .forgejo/workflows/build.yml), which has no windeployqt.
# Copies Liora.exe and every DLL it needs from the MinGW folder by following the DLL imports: Qt,
# the MinGW runtime and Fedora's libraries (zlib, freetype, ...). Each Qt module brings its
# plugins along (see the table below), like windeployqt does. Writes a list of the bundled DLLs
# with their Fedora package and license, and prints the zip's path at the end.
#
# For the Windows 7/8 package (ci/images/fedora44-win7.Dockerfile), which brings its own Qt:
#   QT_ROOT         Qt's folder; its DLLs are taken before the MinGW folder's, and its plugins
#                   from its plugins folder. Its SOURCE.txt, which says where that Qt comes
#                   from, goes into the package as QT-SOURCE.txt.
#   PACKAGE_SUFFIX  the end of the package's name, windows-x64 by default
set -euo pipefail

build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
mingw_root=${MINGW_ROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}
if [[ -n ${QT_ROOT:-} ]]; then
    qt_root=$QT_ROOT
    plugin_root=${QT_PLUGIN_ROOT:-$qt_root/plugins}
else
    qt_root=$mingw_root
    plugin_root=${QT_PLUGIN_ROOT:-$mingw_root/lib/qt6/plugins}
fi
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}
strip=${STRIP:-x86_64-w64-mingw32-strip}
source_dir=$(realpath "$(dirname "$0")/..")

# The plugins each Qt module loads at runtime (paths in the plugin folder, globs allowed). They
# aren't DLL imports, so they can't be found by following those. A Qt module that isn't listed
# stops the script: add it here, with its plugins or with none.
declare -A module_plugins=(
    [Qt6Core]=""
    [Qt6Gui]="platforms/qwindows.dll styles/*.dll imageformats/*.dll iconengines/*.dll"
    [Qt6Widgets]=""
    [Qt6Svg]=""                           # Its plugins are image formats and icon engines (Gui)
    [Qt6Sql]="sqldrivers/qsqlite.dll"     # The other drivers need database client libraries
    [Qt6Network]="tls/*.dll networkinformation/*.dll"
)

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build_dir/CMakeCache.txt")
commit=$(git -C "$source_dir" rev-parse --short HEAD)
name="liora-$version-$commit-${PACKAGE_SUFFIX:-windows-x64}"
stage="$output_dir/$name"
rm -rf "$stage"
mkdir -p "$stage"
cp "$build_dir/Liora.exe" "$stage/"

# Where a DLL the package needs comes from: Qt's folder first, then the MinGW folder (the same
# folder unless QT_ROOT is set). Prints nothing for a DLL in neither.
find_dll() {
    find "$qt_root/bin" "$mingw_root/bin" -maxdepth 1 -iname "$1" -print -quit
}

# Follow the imports of every file in the package. A DLL found in Qt's or the MinGW folder is
# copied (and followed in turn); anything else must be part of Windows itself (KERNEL32.dll, ...).
# A copied Qt module adds its plugins, whose imports are followed too.
declare -A seen=()
system_dlls=()
queue=("$stage/Liora.exe")
add_plugins() {
    local module=$1 pattern plugin patterns
    if [[ ! -v module_plugins[$module] ]]; then
        echo "Liora uses $module, which isn't in the plugin table of $0. Add it there." >&2
        exit 1
    fi
    read -ra patterns <<< "${module_plugins[$module]}"   # Split without expanding the globs here
    for pattern in "${patterns[@]}"; do
        for plugin in "$plugin_root"/$pattern; do
            [[ -e $plugin ]] || continue         # A glob that matched nothing
            plugin=${plugin#"$plugin_root/"}
            mkdir -p "$stage/$(dirname "$plugin")"
            cp "$plugin_root/$plugin" "$stage/$plugin"
            queue+=("$stage/$plugin")
        done
    done
}
while ((${#queue[@]})); do
    file=${queue[0]}
    queue=("${queue[@]:1}")
    while read -r dll; do
        key=${dll,,}
        [[ -n ${seen[$key]:-} ]] && continue
        seen[$key]=1
        found=$(find_dll "$dll")
        if [[ -n $found ]]; then
            cp "$found" "$stage/"
            queue+=("$stage/$(basename "$found")")
            module=$(basename "$found" .dll)
            if [[ $module == Qt6* ]]; then
                add_plugins "$module"
            fi
        else
            system_dlls+=("$dll")
        fi
    done < <("$objdump" -p "$file" | sed -n 's/^\s*DLL Name: //p')
done

# Anything not from Windows that wasn't found would make Liora fail to start on another PC
missing=()
for dll in "${system_dlls[@]}"; do
    case ${dll,,} in
        api-ms-win-*|ext-ms-*|kernel32.dll|user32.dll|gdi32.dll|advapi32.dll|shell32.dll|ole32.dll|\
        oleaut32.dll|comdlg32.dll|ws2_32.dll|winmm.dll|imm32.dll|uxtheme.dll|dwmapi.dll|version.dll|\
        shlwapi.dll|setupapi.dll|netapi32.dll|userenv.dll|wtsapi32.dll|d3d9.dll|d3d11.dll|d3d12.dll|\
        dxgi.dll|dcomp.dll|opengl32.dll|crypt32.dll|bcrypt.dll|secur32.dll|iphlpapi.dll|mpr.dll|\
        msvcrt.dll|ucrtbase.dll|ntdll.dll|authz.dll|dnsapi.dll|comctl32.dll|shcore.dll|usp10.dll|\
        winspool.drv|dwrite.dll|d2d1.dll|propsys.dll|cfgmgr32.dll|hid.dll|powrprof.dll|rpcrt4.dll|\
        mscms.dll)
            ;;
        *) missing+=("$dll") ;;
    esac
done
if ((${#missing[@]})); then
    echo "These DLLs are needed but neither bundled nor part of Windows: ${missing[*]}" >&2
    exit 1
fi

# Remove debug information and symbols, which only a debugger uses. Some of Fedora's DLLs ship
# with them (libstdc++ is 28 MB with, 2 MB without).
find "$stage" -type f \( -iname '*.exe' -o -iname '*.dll' \) -exec "$strip" --strip-unneeded {} +

# Licenses: Liora's own, what's built into Liora.exe, and where each bundled library comes from
cp "$source_dir/LICENSE" "$stage/LICENSE.txt"
cp "$source_dir/LICENSE-LGPL" "$stage/LICENSE-LGPL.txt"
cp "$source_dir/assets/icons/LICENSE" "$stage/LICENSE-icons.txt"
cp "$source_dir/thirdparty/miniz/LICENSE" "$stage/LICENSE-miniz.txt"
own_qt=false
if [[ $qt_root != "$mingw_root" ]]; then
    own_qt=true
    if [[ ! -f $qt_root/SOURCE.txt ]]; then
        echo "$qt_root has no SOURCE.txt saying where this Qt comes from" >&2
        exit 1
    fi
    cp "$qt_root/SOURCE.txt" "$stage/QT-SOURCE.txt"
fi
{
    echo "Liora's source code: https://codeberg.org/spectre_0/aion"
    echo
    echo "Liora.exe contains interface icons based on Tabler Icons (https://tabler.io/icons),"
    echo "under the MIT license in LICENSE-icons.txt."
    echo
    echo "Liora.exe contains miniz (https://github.com/richgel999/miniz), under the MIT license"
    echo "in LICENSE-miniz.txt."
    echo
    echo "Liora bundles these libraries. Each is the Fedora MinGW build named next to it; its"
    echo "source is available from Fedora (https://src.fedoraproject.org/rpms/<package>) and"
    echo "its license applies to that file. Qt is used under the LGPL version 3 (LICENSE-LGPL.txt,"
    echo "which adds to the GPL in LICENSE.txt)."
    if $own_qt; then
        echo "Except Qt, which was built for this package: QT-SOURCE.txt says from which source."
    fi
    echo
    for dll in "$stage"/*.dll "$stage"/*/*.dll; do
        relative=${dll#"$stage/"}
        source_file=$(find_dll "$(basename "$dll")")
        [[ $relative == */* ]] && source_file="$plugin_root/$relative"
        if $own_qt && [[ $source_file == "$qt_root"/* || $relative == */* ]]; then
            echo "$relative: Qt, built for this package (QT-SOURCE.txt)"
            continue
        fi
        package=$(rpm -qf --qf '%{NAME} %{VERSION}, %{LICENSE}' "$source_file" 2>/dev/null || echo "unknown package")
        echo "$relative: $package"
    done
} > "$stage/THIRD-PARTY.txt"

zip="$output_dir/$name.zip"
rm -f "$zip"
(cd "$output_dir" && cmake -E tar cf "$zip" --format=zip "$name")
echo "$zip"
