#define MyAppName "T0G Stream Control"
#define MyAppVersion "0.2.1"
#define MyAppPublisher "T0G Labs"
#ifndef PluginPayload
  #error PluginPayload must be provided by the build workflow
#endif
#ifndef InstallerOutput
  #error InstallerOutput must be provided by the build workflow
#endif

[Setup]
AppId={{6B7C54C4-70C7-4A31-B47D-49C620C5B6E9}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commonpf64}\obs-studio
DisableDirPage=no
UsePreviousAppDir=no
PrivilegesRequired=admin
OutputDir={#InstallerOutput}
OutputBaseFilename=T0G-Stream-Control-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}

[Files]
Source: "{#PluginPayload}\\obs-plugins\\64bit\\t0g-stream-control.dll"; DestDir: "{app}\\obs-plugins\\64bit"; Flags: ignoreversion
Source: "{#PluginPayload}\\data\\obs-plugins\\t0g-stream-control\\*"; DestDir: "{app}\\data\\obs-plugins\\t0g-stream-control"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "T0G-Diagnose.ps1"; DestDir: "{app}\\data\\obs-plugins\\t0g-stream-control"; Flags: ignoreversion

[Code]
function IsOBSInstall(Path: String): Boolean;
begin
  Result := FileExists(AddBackslash(Path) + 'bin\64bit\obs64.exe');
end;

procedure InitializeWizard();
var
  DefaultOBS: String;
begin
  DefaultOBS := ExpandConstant('{commonpf64}\obs-studio');
  if IsOBSInstall(DefaultOBS) then
    WizardForm.DirEdit.Text := DefaultOBS;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then
  begin
    if not IsOBSInstall(WizardDirValue) then
    begin
      MsgBox('OBS Studio was not found in this folder. Select your OBS Studio installation folder (the folder containing bin and obs-plugins).', mbError, MB_OK);
      Result := False;
    end;
  end;
end;

[Run]
Filename: "{sysnative}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\data\obs-plugins\t0g-stream-control\T0G-Diagnose.ps1"" -ObsRoot ""{app}"""; Description: "Run T0G plugin diagnostics"; Flags: postinstall waituntilterminated skipifsilent
Filename: "{app}\bin\64bit\obs64.exe"; Description: "Launch OBS Studio"; Flags: nowait postinstall skipifsilent unchecked
