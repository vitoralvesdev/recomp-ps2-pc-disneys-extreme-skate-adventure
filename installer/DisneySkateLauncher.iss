; Inno Setup script for the native launcher package.
; It installs the launcher, compiled runtime and first-party preparation sources.
; The ISO, extracted game data, derived audio, generated assets, saves and
; third-party tool binaries are NEVER packaged here.
;
; Build: scripts\Empacotar-Instalador.ps1

#define MyAppName "Disney's Extreme Skate Adventure"
#define MyAppVersion "0.3.0"
#define MyAppPublisher "HighSky REC"
#define MyAppURL "https://github.com/vitoralvesdev/recomp-ps2-pc-disneys-extreme-skate-adventure"
#define MyAppExeName "Disney's Extreme Skate Adventure.exe"
#define MyDataDir "{localappdata}\HighSky\DisneySkate"

[Setup]
AppId={{8A9C7A1E-5D5B-4A2C-9B3F-1C7F3D2E6A41}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\HighSky\DisneySkate
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=DisneySkateSetup-{#MyAppVersion}
SetupIconFile=..\src\launcher\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
AppMutex=Local\HighSky_DisneySkate_Launcher_v2
SetupLogging=yes
#ifdef LAUNCHER_TEST_INSTALL
; Build-only variant used to validate the payload inside environments that
; restrict child processes from writing HKCU. Never used for releases.
Uninstallable=no
#endif
VersionInfoVersion=0.3.0.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} launcher setup
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Área de Trabalho"; GroupDescription: "Atalhos:"

; The launcher creates the user data root on first run. Creating it here would
; require write access outside the installation folder before the launcher has
; a chance to choose a custom DISNEY_DATA_ROOT.

[Files]
Source: "..\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\runtime\*"; DestDir: "{app}\runtime"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\launcher.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\docs\controles.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\docs\DISTRIBUTION.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\docs\teste-nativo.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\scripts\Build.Common.psm1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\Preparar-Dados-Nativos.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\Instalar-Audios-Originais.py"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\Recuperar-Trilha-ISO.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "..\scripts\music-recovery\CMakeLists.txt"; DestDir: "{app}\scripts\music-recovery"; Flags: ignoreversion
Source: "..\scripts\music-recovery\install_music.py"; DestDir: "{app}\scripts\music-recovery"; Flags: ignoreversion
Source: "..\scripts\music-recovery\recover_music.cpp"; DestDir: "{app}\scripts\music-recovery"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\Col8Converter.csproj"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\Program.cs"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\ColFile.cs"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\ColFace.cs"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\ColObject.cs"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\ColScene.cs"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\NuGet.Config"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\README.md"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion
Source: "..\tools-src\col8-converter\UPSTREAM-LICENSE.md"; DestDir: "{app}\tools-src\col8-converter"; Flags: ignoreversion

#ifndef LAUNCHER_TEST_INSTALL
[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
#endif

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir o launcher"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The launcher and runtime are removed by uninstall; the data root above is kept.
Type: filesandordirs; Name: "{app}\analysis"
