#!/usr/bin/env bash
# Checks that a packaged Liora for Windows 7/8 finds every Windows function it calls on Windows 7.
#
#   scripts/check-win7-imports.sh <package-dir>
#
# Reads what each .exe and .dll in the package imports, and looks up each function taken from
# Windows in the export lists of Windows 7 SP1 (ci/win7-exports, see the README there). Imports
# from DLLs in the package itself aren't checked. A Windows DLL without an export list fails too:
# add its list. Ends with an error listing every function Windows 7 doesn't have.
set -euo pipefail
export LC_ALL=C                          # objdump's headings in English, sorting by bytes

package=$(realpath "$1")
exports_dir=$(realpath "$(dirname "$0")/../ci/win7-exports")
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}

# Every import as "<dll> <function> <file>", with the DLL name in lower case, and @<ordinal> for
# a function imported by its number. objdump lists them under "DLL Name: <dll>", in one of two
# layouts, named by the line after it:
#   "vma:  Hint/Ord Member-Name"           <address> <hint> <name> | <address> <ordinal, hex> <none>
#   "vma:  Ordinal  Hint  Member-Name"     <address> <none> <hint> <name> | <address> <ordinal> <none> <none>
# (binutils 2.43 and newer). An objdump for Windows ends its lines with \r.
imports=$(mktemp)
trap 'rm -f "$imports"' EXIT
while read -r file; do
    "$objdump" -p "$file" | tr -d '\r' | awk -v file="${file#"$package/"}" '
        /^\tDLL Name: / { dll = tolower($3); next }
        /^\tvma: +Ordinal +Hint/ { newer = 1; next }
        /^\tvma: +Hint\/Ord/ { newer = 0; next }
        NF == 0 { dll = "" }
        dll != "" && /^\t[0-9a-f]+[ \t]/ {
            if (newer) {
                function_name = ($2 == "<none>") ? $4 : "@" ($2 + 0)
            } else {
                function_name = ($3 == "<none>") ? "@" strtonum("0x" $2) : $3
            }
            print dll "\t" function_name "\t" file
        }'
done < <(find "$package" -type f \( -iname '*.exe' -o -iname '*.dll' \) | sort) > "$imports"

# Liora.exe imports from Windows at least, so finding nothing means objdump's output wasn't understood
if [[ ! -s $imports ]]; then
    echo "Found no imports in $package: did objdump's output change?" >&2
    exit 1
fi

declare -A bundled=()
while read -r dll; do
    dll=$(basename "$dll")
    bundled[${dll,,}]=1
done < <(find "$package" -type f -iname '*.dll')

problems=()
while read -r dll; do
    [[ -n ${bundled[$dll]:-} ]] && continue
    list="$exports_dir/$dll.txt"
    if [[ ! -f $list ]]; then
        problems+=("$dll has no Windows 7 export list in ci/win7-exports (imported by $(awk -F'\t' -v dll="$dll" '$1 == dll { print $3 }' "$imports" | sort -u | paste -sd ' '))")
        continue
    fi
    while read -r line; do
        problems+=("$line")
    done < <(awk -F'\t' -v dll="$dll" 'NR == FNR { exported[$0] = 1; next }
                 $1 == dll && !($2 in exported) { print $3 ": " dll " " $2 }' "$list" "$imports" | sort -u)
done < <(cut -f1 "$imports" | sort -u)

if ((${#problems[@]})); then
    echo "Not on Windows 7:" >&2
    printf '  %s\n' "${problems[@]}" >&2
    exit 1
fi
echo "Every Windows function the package imports is on Windows 7 ($(wc -l < "$imports") imports)"
