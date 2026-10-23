#!/usr/bin/env bash
# Packs a MinGW cross-build of Aion into a zip that runs on any Windows PC.
#
#   scripts/package-windows.sh <build-dir> <output-dir>
#
# Runs in the Fedora build image (see .forgejo/workflows/build.yml), which has no windeployqt.
# Copies Aion.exe and every DLL it needs from the MinGW folder by following the DLL imports: Qt,
# the MinGW runtime and Fedora's libraries (zlib, freetype, ...). Each Qt module brings its
# plugins along (see the table below), like windeployqt does. Writes a list of the bundled DLLs
# with their Fedora package and license, and prints the zip's path at the end.
set -euo pipefail

build_dir=$(realpath "$1")
output_dir=$(realpath -m "$2")
mingw_root=${MINGW_ROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}
plugin_root=${QT_PLUGIN_ROOT:-$mingw_root/lib/qt6/plugins}
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}
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
name="aion-$version-$commit-windows-x64"
stage="$output_dir/$name"
rm -rf "$stage"
mkdir -p "$stage"
cp "$build_dir/Aion.exe" "$stage/"

# Follow the imports of every file in the package. A DLL found in the MinGW folder is copied
# (and followed in turn); anything else must be part of Windows itself (KERNEL32.dll, ...). A
# copied Qt module adds its plugins, whose imports are followed too.
declare -A seen=()
system_dlls=()
queue=("$stage/Aion.exe")
add_plugins() {
    local module=$1 pattern plugin patterns
    if [[ ! -v module_plugins[$module] ]]; then
        echo "Aion uses $module, which isn't in the plugin table of $0. Add it there." >&2
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
        found=$(find "$mingw_root/bin" -maxdepth 1 -iname "$dll" -print -quit)
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

# Anything not from Windows that wasn't found would make Aion fail to start on another PC
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

# Licenses: Aion's own, the icons built into Aion.exe, and where each bundled library comes from
cp "$source_dir/LICENSE" "$stage/LICENSE.txt"
cp "$source_dir/assets/icons/LICENSE" "$stage/LICENSE-icons.txt"
{
    echo "Aion.exe contains interface icons based on Tabler Icons (https://tabler.io/icons),"
    echo "under the MIT license in LICENSE-icons.txt."
    echo
    echo "Aion bundles these libraries. Each is the Fedora MinGW build named next to it; its"
    echo "source is available from Fedora (https://src.fedoraproject.org/rpms/<package>) and"
    echo "its license applies to that file. Qt is used under the LGPL version 3."
    echo
    for dll in "$stage"/*.dll "$stage"/*/*.dll; do
        relative=${dll#"$stage/"}
        source_file="$mingw_root/bin/$(basename "$dll")"
        [[ $relative == */* ]] && source_file="$plugin_root/$relative"
        package=$(rpm -qf --qf '%{NAME} %{VERSION}, %{LICENSE}' "$source_file" 2>/dev/null || echo "unknown package")
        echo "$relative: $package"
    done
} > "$stage/THIRD-PARTY.txt"

zip="$output_dir/$name.zip"
rm -f "$zip"
(cd "$output_dir" && cmake -E tar cf "$zip" --format=zip "$name")
echo "$zip"
