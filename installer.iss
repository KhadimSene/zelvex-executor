; Inno Setup script for Zelvex
; To compile: iscc installer.iss

#define MyAppName "Zelvex"
#define MyAppVersion "4.0.0"
#define MyAppPublisher "Zelvex"
#define MyAppURL ""
#define MyAppExeName "Zelvex.exe"

[Setup]
AppId={{F8A7B3C2-5D4E-4F6A-9B8C-1D2E3F4A5B6C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=build\installer
OutputBaseFileName=ZelvexSetup-{#MyAppVersion}
SetupIconFile=src\images\logo.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: checkedonce

[Files]
Source: "build\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\dist\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\dist\lua_dll_new.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\dist\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\dist\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\dist\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\dist\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\dist\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\dist\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Zelvex"; Flags: postinstall nowait skipifsilent shellexec
