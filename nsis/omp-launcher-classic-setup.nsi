Unicode true
ManifestSupportedOS all
RequestExecutionLevel user
SetCompressor /FINAL /SOLID lzma
ShowInstDetails show
ShowUnInstDetails show
CRCCheck on

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef APP_NAME
  !define APP_NAME "open.mp Classic"
!endif

!ifndef APP_VERSION
  !define APP_VERSION "working"
!endif

!ifndef APP_PUBLISHER
  !define APP_PUBLISHER "open.mp Classic"
!endif

!ifndef DIST_DIR
  !error "DIST_DIR must be defined"
!endif

!ifndef OUT_FILE
  !error "OUT_FILE must be defined"
!endif

!ifndef APP_ICON
  !error "APP_ICON must be defined"
!endif

!define APP_EXE "omp-launcher-classic.exe"
!define UNINSTALL_EXE "Uninstall.exe"
!define STARTMENU_DIR "${APP_NAME}"
!define INSTALL_DIR_NAME "Open Multiplayer Launcher Classic"
!define UNINSTALL_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
!define OMP_CLIENT_URL "https://assets.open.mp/omp-client.dll"

Name "${APP_NAME}"
OutFile "${OUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\${INSTALL_DIR_NAME}"
InstallDirRegKey HKCU "${UNINSTALL_REG_KEY}" "InstallLocation"
BrandingText "${APP_NAME} ${APP_VERSION}"
XPStyle on

!define MUI_ABORTWARNING
!define MUI_ICON "${APP_ICON}"
!define MUI_UNICON "${APP_ICON}"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${APP_NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK "${APP_NAME} requires a 64-bit version of Windows."
    Abort
  ${EndIf}
  SetShellVarContext current
  SetRegView 64
FunctionEnd

Section "Install"
  SetOutPath "$INSTDIR"
  File /r "${DIST_DIR}\*"

  WriteUninstaller "$INSTDIR\${UNINSTALL_EXE}"

  CreateDirectory "$SMPROGRAMS\${STARTMENU_DIR}"
  CreateShortcut "$SMPROGRAMS\${STARTMENU_DIR}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
  CreateShortcut "$SMPROGRAMS\${STARTMENU_DIR}\Uninstall.lnk" "$INSTDIR\${UNINSTALL_EXE}"
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"

  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "UninstallString" "$\"$INSTDIR\${UNINSTALL_EXE}$\""
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoRepair" 1

  MessageBox MB_ICONINFORMATION|MB_OK "Due to licensing, omp-client.dll is not included.$\r$\n$\r$\nDownload it manually from:$\r$\n${OMP_CLIENT_URL}$\r$\n$\r$\nThen place omp-client.dll in:$\r$\n$INSTDIR"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${STARTMENU_DIR}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${STARTMENU_DIR}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${STARTMENU_DIR}"

  DeleteRegKey HKCU "${UNINSTALL_REG_KEY}"

  RMDir /r "$INSTDIR"
SectionEnd
