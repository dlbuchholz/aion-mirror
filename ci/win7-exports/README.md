# Windows 7 export lists

What the Windows system DLLs of Windows 7 SP1 export, one list per DLL (`<dll>.txt`: names, and
`@<ordinal>` for each ordinal). `scripts/check-win7-imports.sh` looks up every Windows function
the Windows 7/8 package imports in these lists, so a function Windows 7 doesn't have fails the
build instead of the start on a user's PC.

Made from the DLLs in `C:\Windows\System32` of a Windows 7 SP1 64-bit PC with these updates
installed (`wmic qfe list brief`): KB976902, KB976932 (Service Pack 1), KB3020369, KB3125574
(convenience rollup), KB3138612, KB4474419, KB4490628. Only the lists are kept here, not the DLLs.

When the check names a Windows DLL that has no list yet, copy that DLL from the same kind of PC,
next to the others, and make the lists again:

    scripts/win7-exports.sh <folder with the DLLs> ci/win7-exports
