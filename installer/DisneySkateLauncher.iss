#ifndef MyAppVersion
  #define MyAppVersion "0.5.0"
#endif
#ifndef VersionInfoVersion
  #define VersionInfoVersion "0.5.0.0"
#endif
#ifndef InstallerSuffix
  #define InstallerSuffix ""
#endif
#ifndef PackageRoot
  #error "PackageRoot deve apontar para o staging validado."
#endif
#ifndef DistDir
  #define DistDir "..\dist"
#endif
#ifndef AppIcon
  #define AppIcon "..\src\launcher\app.ico"
#endif

#define MyAppName "Disney's Extreme Skate Adventure"
#define MyAppPublisher "HighSky REC"
#define MyAppURL "https://github.com/vitoralvesdev/recomp-ps2-pc-disneys-extreme-skate-adventure"
#define MyAppExeName "Disney's Extreme Skate Adventure.exe"

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
OutputDir={#DistDir}
OutputBaseFilename=DisneySkateSetup-{#MyAppVersion}{#InstallerSuffix}
SetupIconFile={#AppIcon}
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
Uninstallable=no
#endif
VersionInfoVersion={#VersionInfoVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} launcher setup
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Área de Trabalho"; GroupDescription: "Atalhos:"

[Files]
Source: "{#PackageRoot}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\BUILD-INFO.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\package-files.sha256"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageRoot}\runtime\DisneySkateRuntime.exe"; DestDir: "{app}\runtime"; Flags: ignoreversion
Source: "{#PackageRoot}\runtime\*.dll"; DestDir: "{app}\runtime"; Flags: ignoreversion
Source: "{#PackageRoot}\scripts\Build.Common.psm1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "{#PackageRoot}\scripts\Preparar-Dados-Nativos.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "{#PackageRoot}\scripts\Instalar-Audios-Originais.py"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "{#PackageRoot}\scripts\Recuperar-Trilha-ISO.ps1"; DestDir: "{app}\scripts"; Flags: ignoreversion
Source: "{#PackageRoot}\scripts\music-recovery\install_music.py"; DestDir: "{app}\scripts\music-recovery"; Flags: ignoreversion
Source: "{#PackageRoot}\docs\*.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#PackageRoot}\tools\python\*"; DestDir: "{app}\tools\python"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageRoot}\tools\col8-converter\*"; DestDir: "{app}\tools\col8-converter"; Flags: ignoreversion
Source: "{#PackageRoot}\tools\music-recovery\DisneyMusicRecover.exe"; DestDir: "{app}\tools\music-recovery"; Flags: ignoreversion
Source: "{#PackageRoot}\tools\neversoft-multitool-v1.3.4\cli\NeversoftMultitool.exe"; DestDir: "{app}\tools\neversoft-multitool-v1.3.4\cli"; Flags: ignoreversion
Source: "{#PackageRoot}\tools\neversoft-multitool-v1.3.4\LICENSE.md"; DestDir: "{app}\tools\neversoft-multitool-v1.3.4"; Flags: ignoreversion
Source: "{#PackageRoot}\third-party-notices\*"; DestDir: "{app}\third-party-notices"; Flags: ignoreversion recursesubdirs createallsubdirs

#ifndef LAUNCHER_TEST_INSTALL
[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
#endif

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Abrir o launcher"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}\analysis"
