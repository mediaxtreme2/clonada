; Clonada AI Vocal Suite - Inno Setup Script
; Single-step installer: plugins + standalone + AI engine

#define AppName "Clonada"
#ifndef AppVersion
  #define AppVersion "1.2.0"
#endif
#define AppPublisher "mediaXtreme LLC"
#define AppURL "https://github.com/anirudhatalmale6-alt/clonada"

[Setup]
AppId={{A3F2C7E1-8B4D-4E9A-B5C6-1D2E3F4A5B6C}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=Clonada-{#AppVersion}-Windows-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=license.txt
SetupIconFile=clonada.ico
UninstallDisplayName=Clonada AI Vocal Suite
UninstallDisplayIcon={app}\clonada.ico
WizardImageStretch=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Types]
Name: "full"; Description: "Full installation (Plugin + Standalone + AI Engine) — Recommended"
Name: "plugin"; Description: "Plugin only (VST3/CLAP, no AI engine)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full plugin custom; Flags: fixed
Name: "clap"; Description: "CLAP Plugin"; Types: full plugin custom
Name: "standalone"; Description: "Standalone Application"; Types: full custom
Name: "engine"; Description: "AI Engine (voice cloning, stem separation)"; Types: full custom

[Files]
; VST3 plugin
Source: "..\plugin\build\Clonada_artefacts\Release\VST3\Clonada.vst3\*"; DestDir: "{commoncf64}\VST3\Clonada.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; CLAP plugin
Source: "..\plugin\build\Clonada_artefacts\Release\CLAP\Clonada.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion

; Standalone
Source: "..\plugin\build\Clonada_artefacts\Release\Standalone\Clonada.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

; Engine files (Python sidecar)
Source: "..\python\*.py"; DestDir: "{app}\engine"; Components: engine; Flags: ignoreversion
Source: "..\python\requirements.txt"; DestDir: "{app}\engine"; Components: engine; Flags: ignoreversion
Source: "..\python\lib\*.py"; DestDir: "{app}\engine\lib"; Components: engine; Flags: ignoreversion

; Installer scripts and assets
Source: "install_windows.bat"; DestDir: "{app}"; Components: engine; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "clonada.ico"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Icons]
Name: "{group}\Clonada Standalone"; Filename: "{app}\Clonada.exe"; IconFilename: "{app}\clonada.ico"; Components: standalone
Name: "{group}\Clonada AI Engine"; Filename: "{userappdata}\..\Clonada\start_engine.bat"; IconFilename: "{app}\clonada.ico"; Components: engine
Name: "{group}\Activate License"; Filename: "{userappdata}\..\Clonada\activate_license.bat"; IconFilename: "{app}\clonada.ico"; Components: engine
Name: "{group}\Uninstall Clonada"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Clonada"; Filename: "{app}\Clonada.exe"; IconFilename: "{app}\clonada.ico"; Components: standalone; Tasks: desktopicon

[Registry]
Root: HKLM; Subkey: "SOFTWARE\mediaXtreme\Clonada"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\mediaXtreme\Clonada"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"

[Run]
; Auto-run AI engine setup after install (passes install dir as argument)
Filename: "{app}\install_windows.bat"; Parameters: """{app}"""; Description: "Setup AI Engine now (recommended — downloads ~2 GB, takes 5-15 min)"; Components: engine; Flags: postinstall shellexec

[UninstallDelete]
Type: filesandordirs; Name: "{app}\engine"

[UninstallRun]
; Clean up Clonada home directory on uninstall
Filename: "cmd.exe"; Parameters: "/c rd /s /q ""%USERPROFILE%\Clonada"""; Flags: runhidden

[Code]
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
  begin
    if not DirExists(ExpandConstant('{commoncf64}\VST3')) then
      ForceDirectories(ExpandConstant('{commoncf64}\VST3'));
    if not DirExists(ExpandConstant('{commoncf64}\CLAP')) then
      ForceDirectories(ExpandConstant('{commoncf64}\CLAP'));
  end;
end;
