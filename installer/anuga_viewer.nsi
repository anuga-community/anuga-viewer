; NSIS Modern UI installer for ANUGA Viewer
; Run from the repo root:  makensis installer\anuga_viewer.nsi

!include "MUI2.nsh"

; ---------------------------------------------------------------------------
; Defines — VERSION is injected by CI via /DVERSION=x.y.z
; ---------------------------------------------------------------------------
!ifndef VERSION
  !define VERSION "dev"
!endif

!define APP_NAME      "ANUGA Viewer"
!define APP_EXE       "anuga_viewer.exe"
!define APP_REGKEY    "Software\ANUGAViewer"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ANUGAViewer"

Name    "${APP_NAME} ${VERSION}"
OutFile "anuga-viewer-windows-setup.exe"

InstallDir      "$PROGRAMFILES64\ANUGA Viewer"
InstallDirRegKey HKCU "${APP_REGKEY}" "InstallDir"
RequestExecutionLevel admin

; ---------------------------------------------------------------------------
; MUI appearance
; ---------------------------------------------------------------------------
!define MUI_ICON   "distros\icon.ico"
!define MUI_UNICON "distros\icon.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN        "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT   "Launch ANUGA Viewer now"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------------------
; Installer section
; ---------------------------------------------------------------------------
Section "ANUGA Viewer" SecMain
  SectionIn RO

  SetOutPath "$INSTDIR"
  File /r "dist\anuga-viewer\*.*"

  ; SWOLLEN_BINDIR tells the viewer where to find bundled fonts and images.
  ; Write it as a persistent user environment variable so command-line
  ; invocations (not just shortcuts) also work.
  WriteRegExpandStr HKCU "Environment" "SWOLLEN_BINDIR" "$INSTDIR"
  ; Broadcast the change so already-running processes (Explorer) pick it up.
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  ; Remember install location for the uninstaller and future upgrades.
  WriteRegStr HKCU "${APP_REGKEY}" "InstallDir" "$INSTDIR"

  ; Register in Windows "Apps & features" / Add-Remove Programs.
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayName"     "${APP_NAME}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayVersion"  "${VERSION}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "Publisher"       "ANUGA Community"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "DisplayIcon"     "$INSTDIR\${APP_EXE}"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "URLInfoAbout"    "https://github.com/anuga-community/anuga-viewer"
  WriteRegStr   HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify"        1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair"        1

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Start Menu shortcuts
  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortCut  "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" \
                  "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}"
  CreateShortCut  "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" \
                  "$INSTDIR\Uninstall.exe"

  ; Desktop shortcut
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk" \
                 "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}"
SectionEnd

; ---------------------------------------------------------------------------
; Optional: .sww file association
; ---------------------------------------------------------------------------
Section "Associate .sww files with ANUGA Viewer" SecAssoc
  WriteRegStr HKCU "Software\Classes\.sww"                              "" "ANUGAViewer.swwfile"
  WriteRegStr HKCU "Software\Classes\ANUGAViewer.swwfile"               "" "ANUGA Simulation File"
  WriteRegStr HKCU "Software\Classes\ANUGAViewer.swwfile\DefaultIcon"   "" "$INSTDIR\${APP_EXE},0"
  WriteRegStr HKCU "Software\Classes\ANUGAViewer.swwfile\shell\open\command" \
                   "" '"$INSTDIR\${APP_EXE}" "%1"'
SectionEnd

; ---------------------------------------------------------------------------
; Uninstaller
; ---------------------------------------------------------------------------
Section "Uninstall"
  RMDir /r "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
  RMDir  "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  DeleteRegKey   HKCU "${APP_REGKEY}"
  DeleteRegKey   HKLM "${UNINSTALL_KEY}"
  DeleteRegValue HKCU "Environment" "SWOLLEN_BINDIR"
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  DeleteRegKey HKCU "Software\Classes\.sww"
  DeleteRegKey HKCU "Software\Classes\ANUGAViewer.swwfile"
SectionEnd
