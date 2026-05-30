; Clonada AI Vocal Suite - Inno Setup Script
; Packages VST3/CLAP plugin + Python engine sidecar

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

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Types]
Name: "full"; Description: "Full installation (Plugin + Engine)"
Name: "plugin"; Description: "Plugin only (VST3/CLAP)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full plugin custom; Flags: fixed
Name: "clap"; Description: "CLAP Plugin"; Types: full custom
Name: "standalone"; Description: "Standalone Application"; Types: full custom
Name: "engine"; Description: "AI Engine (Python sidecar)"; Types: full custom

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

; Launcher scripts
Source: "install_windows.bat"; DestDir: "{app}"; Components: engine; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Clonada Standalone"; Filename: "{app}\Clonada.exe"; Components: standalone
Name: "{group}\Setup AI Engine"; Filename: "{app}\install_windows.bat"; Components: engine
Name: "{group}\Uninstall Clonada"; Filename: "{uninstallexe}"

[Registry]
Root: HKLM; Subkey: "SOFTWARE\mediaXtreme\Clonada"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\mediaXtreme\Clonada"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"

[Run]
Filename: "{app}\install_windows.bat"; Description: "Setup AI Engine now (recommended — takes 5-15 min)"; Components: engine; Flags: postinstall shellexec

[UninstallDelete]
Type: filesandordirs; Name: "{app}\engine"
Type: filesandordirs; Name: "{app}\weights"
Type: filesandordirs; Name: "{app}\models"

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
