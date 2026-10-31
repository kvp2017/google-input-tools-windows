#define AppVersion "0.2.0"
#define BuildDir "..\build\Release"

[Setup]
AppId={{9D6EEC9A-8F55-4E1B-ACF5-6048D1102A39}
AppName=Tamil Transliteration for Windows
AppVersion={#AppVersion}
AppPublisher=KVP Community
DefaultDirName={localappdata}\Programs\KVP\TamilTransliteration
DefaultGroupName=Tamil Transliteration
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=..\artifacts
OutputBaseFilename=TamilTransliteration-Setup-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\TamilImeSettings.exe

[Files]
Source: "{#BuildDir}\TamilTextService.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\TamilImeRegistration.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\TamilImeSettings.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Tamil Input Settings"; Filename: "{app}\TamilImeSettings.exe"
Name: "{userdesktop}\Tamil Input Settings"; Filename: "{app}\TamilImeSettings.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for Tamil Input Settings"; Flags: unchecked

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  Params: String;
begin
  if CurStep = ssPostInstall then
  begin
    Params := '/register "' + ExpandConstant('{app}\TamilTextService.dll') + '"';
    if not Exec(ExpandConstant('{app}\TamilImeRegistration.exe'), Params, '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode) or (ResultCode <> 0) then
    begin
      MsgBox('Tamil input method registration failed. The installer will stop; please retry after checking Windows Text Services.', mbError, MB_OK);
      Abort;
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{app}\TamilImeRegistration.exe'), '/unregister', '',
      SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
