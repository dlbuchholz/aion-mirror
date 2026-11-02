#!/usr/bin/env bash
# Writes down what Windows system DLLs export, one list per DLL, for scripts/check-win7-imports.sh.
#
#   scripts/win7-exports.sh <folder with the DLLs> <output-dir>
#
# Run with system DLLs copied from a Windows 7 SP1 PC (C:\Windows\System32) to make
# ci/win7-exports/ (see the README there). Each list, <dll>.txt, has the exported names and
# @<ordinal> for each exported ordinal, one per line.
set -euo pipefail
export LC_ALL=C                          # objdump's headings in English, sorting by bytes

dll_dir=$1
output_dir=$2
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}

mkdir -p "$output_dir"
for dll in "$dll_dir"/*; do
    case ${dll,,} in
        *.dll|*.drv) ;;
        *) continue ;;
    esac
    name=$(basename "$dll")
    # objdump lists the names under "[Ordinal/Name Pointer] Table", the name last on each line
    # ("[   0] AddAtomA", or "[   0] +base[   1]  0000 AddAtomA" since binutils 2.43), and the
    # ordinals under "Export Address Table" ("[   0] +base[   1] 45c0 Export RVA"). An objdump
    # for Windows ends its lines with \r.
    "$objdump" -p "$dll" | tr -d '\r' | awk '
        /^\[Ordinal\/Name Pointer\] Table/ { names = 1; next }
        /^Export Address Table -- Ordinal Base/ { ordinals = 1; next }
        !/^\t/ { names = 0; ordinals = 0 }
        names && /^\t\[ *[0-9]+\] / { print $NF }
        ordinals && match($0, /\+base\[ *[0-9]+\]/) {
            ordinal = substr($0, RSTART + 6, RLENGTH - 7)
            gsub(/ /, "", ordinal)
            print "@" ordinal
        }' | sort -u > "$output_dir/${name,,}.txt"
    if [[ ! -s $output_dir/${name,,}.txt ]]; then
        echo "Found no exports in $dll" >&2
        exit 1
    fi
done
