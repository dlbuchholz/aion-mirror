; Windows installer for Aion (NSIS 3). Built by CI from the packaged folder:
;
;   makensis -DVERSION=0.1.0 -DSOURCE=<package folder> -DICON=<aion.ico> -DOUTFILE=<setup.exe> aion.nsi
;
; Installs for the current user only (no admin rights) into %LOCALAPPDATA%\Programs\Aion, with a
; Start menu entry and an optional desktop shortcut. Uninstalling removes the program but never
; the user's data (database and settings live elsewhere).

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION | SOURCE | ICON | OUTFILE
    !error "Pass VERSION, SOURCE, ICON and OUTFILE with -D (see the top of this file)"
!endif

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aion"

Name "Aion"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\Aion"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
BrandingText "Aion ${VERSION}"
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Aion"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Aion installer"
VIAddVersionKey "LegalCopyright" "The Aion contributors"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_DIRECTORYPAGE_TEXT_TOP "Aion will be installed for your user account. The folder must be named Aion, so uninstalling can remove it safely."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Aion.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start Aion"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE}/LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Installing and uninstalling delete the whole folder, so it must be Aion's own, never the
; Desktop or Documents: its name must be Aion. Checked on the directory page, and again before
; deleting, since a silent install (/S /D=...) skips that page.
!macro CheckAionFolder
    StrCpy $0 "$INSTDIR" "" -5
    StrCmp $0 "\Aion" +3
        MessageBox MB_ICONSTOP "The folder must be named Aion: $INSTDIR" /SD IDOK
        Abort
!macroend

Function .onVerifyInstDir
    StrCpy $0 "$INSTDIR" "" -5
    StrCmp $0 "\Aion" +2
        Abort
FunctionEnd

Section "Aion" SecAion
    SectionIn RO
    !insertmacro CheckAionFolder
    ; Remove the previous version's files first, so no outdated DLL stays behind
    RMDir /r "$INSTDIR"
    SetOutPath "$INSTDIR"
    File /r "${SOURCE}/*.*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateShortcut "$SMPROGRAMS\Aion.lnk" "$INSTDIR\Aion.exe"

    ; Entry under Settings → Apps
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Aion"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "The Aion contributors"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Aion.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" $0
SectionEnd

Section /o "Desktop shortcut" SecDesktop
    CreateShortcut "$DESKTOP\Aion.lnk" "$INSTDIR\Aion.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAion} "The program, with a Start menu entry."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A shortcut to Aion on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Removes the program, its shortcuts and its Apps entry. The database and settings stay.
Section "Uninstall"
    !insertmacro CheckAionFolder
    Delete "$SMPROGRAMS\Aion.lnk"
    Delete "$DESKTOP\Aion.lnk"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR"                              ; Checked above: a folder named Aion
SectionEnd
