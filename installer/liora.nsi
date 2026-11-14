; Windows installer for Liora (NSIS 3). Built by CI from the packaged folder:
;
;   makensis -DVERSION=0.1.0 -DSOURCE=<package folder> -DICON=<liora.ico> -DOUTFILE=<setup.exe> liora.nsi
;
; Installs for the current user only (no admin rights) into %LOCALAPPDATA%\Programs\Liora, with a
; Start menu entry and an optional desktop shortcut. Uninstalling removes the program, and the
; user's data (database and settings) only when they tick that option.

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!ifndef VERSION | SOURCE | ICON | OUTFILE
    !error "Pass VERSION, SOURCE, ICON and OUTFILE with -D (see the top of this file)"
!endif

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Liora"

Name "Liora"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\Liora"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
BrandingText "Liora ${VERSION}"
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Liora"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Liora installer"
VIAddVersionKey "LegalCopyright" "The Liora contributors"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_DIRECTORYPAGE_TEXT_TOP "Liora will be installed for your user account. The folder must be named Liora, so uninstalling can remove it safely."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Liora.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start Liora"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SOURCE}/LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Installing and uninstalling delete the whole folder, so it must be Liora's own, never the
; Desktop or Documents: its name must be Liora. Checked on the directory page, and again before
; deleting, since a silent install (/S /D=...) skips that page.
!macro CheckLioraFolder
    StrCpy $0 "$INSTDIR" "" -6
    StrCmp $0 "\Liora" +3
        MessageBox MB_ICONSTOP "The folder must be named Liora: $INSTDIR" /SD IDOK
        Abort
!macroend

Function .onVerifyInstDir
    StrCpy $0 "$INSTDIR" "" -6
    StrCmp $0 "\Liora" +2
        Abort
FunctionEnd

Section "Liora" SecLiora
    SectionIn RO
    !insertmacro CheckLioraFolder
    ; Remove the previous version's files first, so no outdated DLL stays behind
    RMDir /r "$INSTDIR"
    SetOutPath "$INSTDIR"
    File /r "${SOURCE}/*.*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    CreateShortcut "$SMPROGRAMS\Liora.lnk" "$INSTDIR\Liora.exe"

    ; Entry under Settings → Apps
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Liora"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "The Liora contributors"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Liora.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "EstimatedSize" $0
SectionEnd

Section /o "Desktop shortcut" SecDesktop
    CreateShortcut "$DESKTOP\Liora.lnk" "$INSTDIR\Liora.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecLiora} "The program, with a Start menu entry."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A shortcut to Liora on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Removes the program, its shortcuts and its Apps entry
Section "un.Liora" SecUnLiora
    SectionIn RO
    !insertmacro CheckLioraFolder
    Delete "$SMPROGRAMS\Liora.lnk"
    Delete "$DESKTOP\Liora.lnk"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR"                              ; Checked above: a folder named Liora
SectionEnd

; Only when ticked (never in a silent uninstall): Liora's own data. Where Liora keeps it: the app
; data folder (QStandardPaths::AppDataLocation, the default database) and the registry key of
; its QSettings("LeineHive", "Liora"). Databases saved elsewhere aren't Liora's to delete.
Section /o "un.My data" SecUnData
    RMDir /r "$APPDATA\Liora"
    DeleteRegKey HKCU "Software\LeineHive\Liora"
    DeleteRegKey /ifempty HKCU "Software\LeineHive"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnLiora} "The program, its shortcuts and its entry in Settings > Apps."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnData} "Also deletes your data for good: the database in $APPDATA\Liora and all settings. Databases you saved in other folders are kept."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END
