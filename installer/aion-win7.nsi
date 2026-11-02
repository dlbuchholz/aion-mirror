; Windows installer for the Windows 7/8 build of Aion (NSIS 3). Built by CI from the packaged
; folder, like aion.nsi:
;
;   makensis -DVERSION=0.1.1 -DSOURCE=<package folder> -DICON=<aion.ico> -DOUTFILE=<setup.exe> aion-win7.nsi
;
; A copy of aion.nsi with its own name, folder, shortcuts and Apps entry, so it never touches an
; installed regular Aion (Windows 10/11). Keep changes to aion.nsi in step here. It also stops on
; Windows older than 7 SP1 and on 32-bit Windows, which the package doesn't run on.
;
; Installs for the current user only (no admin rights) into %LOCALAPPDATA%\Programs\Aion-Win7,
; with a Start menu entry and an optional desktop shortcut. Uninstalling removes the program, and
; the user's data (database and settings, shared with the regular Aion) only when they tick that
; option.

Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

!ifndef VERSION | SOURCE | ICON | OUTFILE
    !error "Pass VERSION, SOURCE, ICON and OUTFILE with -D (see the top of this file)"
!endif

!define APP_NAME "Aion for Windows 7 and 8"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aion-Win7"
!define FOLDER_NAME "Aion-Win7"                    ; No space: Wine quotes a /D=... with one, which NSIS ignores

Name "${APP_NAME}"
OutFile "${OUTFILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\${FOLDER_NAME}"
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
BrandingText "Aion ${VERSION}"
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APP_NAME}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "Aion installer for Windows 7 and 8"
VIAddVersionKey "LegalCopyright" "The Aion contributors"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_DIRECTORYPAGE_TEXT_TOP "Aion will be installed for your user account. The folder must be named ${FOLDER_NAME}, so uninstalling can remove it safely."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Aion.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start Aion"

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

; This build is 64-bit and needs Windows 7 with Service Pack 1 or newer
Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "${APP_NAME} needs 64-bit Windows." /SD IDOK
        Abort
    ${EndIf}
    ${IfNot} ${AtLeastWin7}
        MessageBox MB_ICONSTOP "${APP_NAME} needs Windows 7 with Service Pack 1 or newer." /SD IDOK
        Abort
    ${EndIf}
    ${If} ${IsWin7}
    ${AndIfNot} ${AtLeastServicePack} 1
        MessageBox MB_ICONSTOP "${APP_NAME} needs Service Pack 1 for Windows 7." /SD IDOK
        Abort
    ${EndIf}
FunctionEnd

; Installing and uninstalling delete the whole folder, so it must be Aion's own, never the
; Desktop or Documents: its name must be ${FOLDER_NAME}. Checked on the directory page, and again
; before deleting, since a silent install (/S /D=...) skips that page.
!macro CheckAionFolder
    StrCpy $0 "$INSTDIR" "" -10
    StrCmp $0 "\${FOLDER_NAME}" +3
        MessageBox MB_ICONSTOP "The folder must be named ${FOLDER_NAME}: $INSTDIR" /SD IDOK
        Abort
!macroend

Function .onVerifyInstDir
    StrCpy $0 "$INSTDIR" "" -10
    StrCmp $0 "\${FOLDER_NAME}" +2
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

    CreateShortcut "$SMPROGRAMS\${APP_NAME}.lnk" "$INSTDIR\Aion.exe"

    ; Entry under Control Panel > Programs and Features (Settings > Apps on newer Windows)
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP_NAME}"
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
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\Aion.exe"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAion} "The program, with a Start menu entry."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "A shortcut to Aion on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Removes the program, its shortcuts and its entry in the list of programs
Section "un.Aion" SecUnAion
    SectionIn RO
    !insertmacro CheckAionFolder
    Delete "$SMPROGRAMS\${APP_NAME}.lnk"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR"                              ; Checked above: a folder named ${FOLDER_NAME}
SectionEnd

; Only when ticked (never in a silent uninstall): Aion's own data. Where Aion keeps it: the app
; data folder (QStandardPaths::AppDataLocation, the default database) and the registry key of
; its QSettings("LeineHive", "Aion"). Databases saved elsewhere aren't Aion's to delete. The
; regular Aion uses the same data.
Section /o "un.My data" SecUnData
    RMDir /r "$APPDATA\Aion"
    DeleteRegKey HKCU "Software\LeineHive\Aion"
    DeleteRegKey /ifempty HKCU "Software\LeineHive"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnAion} "The program, its shortcuts and its entry in the list of programs."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUnData} "Also deletes your data for good: the database in $APPDATA\Aion and all settings, which a regular Aion on this PC uses too. Databases you saved in other folders are kept."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END
