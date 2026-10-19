#ifndef CompilerVersion
  #error CompilerVersion is required
#endif
#if DecodeVer(Ver) != CompilerVersion
  #error The Inno Setup compiler version does not match versions.conf
#endif

#ifndef ProductVersion
  #error ProductVersion is required
#endif
#ifndef DistDir
  #error DistDir is required
#endif
#ifndef WorkDir
  #error WorkDir is required
#endif
#ifndef AssetsDir
  #error AssetsDir is required
#endif
#ifndef UninstallActions
  #error UninstallActions is required
#endif

[Setup]
AppId={{D7D7F47A-9CB2-46D1-8A68-A2A062F028D5}
AppName=Ludork
AppVersion={#ProductVersion}
AppPublisher=Ludork
AppPublisherURL=https://github.com/JasonLeon01/Ludork
AppSupportURL=https://github.com/JasonLeon01/Ludork/issues
DefaultDirName={autopf}\Ludork
DefaultGroupName=Ludork
AllowNoIcons=yes
DisableProgramGroupPage=yes
DisableDirPage=no
DisableWelcomePage=no
LicenseFile={#WorkDir}\License.en.txt
SetupIconFile={#AssetsDir}\icon.ico
UninstallDisplayIcon={app}\Ludork.exe
Uninstallable=yes
UninstallDisplayName=Ludork
UninstallFilesDir={app}
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#WorkDir}
OutputBaseFilename=Ludork-{#ProductVersion}-windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no
UsePreviousTasks=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "officialplugins"; Description: "Install Official Plugins (replace all existing plug-ins and their data)"; GroupDescription: "User data:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#WorkDir}\InstallerPayload.txt"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AssetsDir}\project-icon.ico"; DestDir: "{app}"; DestName: "LudorkProject.ico"; Flags: ignoreversion
Source: "{#UninstallActions}"; DestDir: "{app}\tools"; DestName: "LudorkUninstallActions.dll"; Flags: ignoreversion

[Icons]
Name: "{group}\Ludork"; Filename: "{app}\Ludork.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Ludork"; Filename: "{app}\Ludork.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKLM; Subkey: "Software\Ludork\Installer"; ValueType: string; ValueName: "Format"; ValueData: "exe"; Flags: uninsdeletevalue uninsdeletekeyifempty
Root: HKLM; Subkey: "Software\Ludork\Installer"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletevalue uninsdeletekeyifempty
Root: HKLM; Subkey: "Software\Ludork\Installer"; ValueType: string; ValueName: "Version"; ValueData: "{#ProductVersion}"; Flags: uninsdeletevalue uninsdeletekeyifempty
Root: HKLM; Subkey: "Software\Classes\.proj"; ValueType: string; ValueData: "Ludork.Project"
Root: HKLM; Subkey: "Software\Classes\Ludork.Project"; ValueType: string; ValueData: "Ludork Project"
Root: HKLM; Subkey: "Software\Classes\Ludork.Project\DefaultIcon"; ValueType: string; ValueData: """{app}\LudorkProject.ico"",0"
Root: HKLM; Subkey: "Software\Classes\Ludork.Project\shell\open"; ValueType: string; ValueData: "Open"
Root: HKLM; Subkey: "Software\Classes\Ludork.Project\shell\open\command"; ValueType: string; ValueData: """{app}\Ludork.exe"" ""%1"""

[Run]
Filename: "{app}\Ludork.exe"; Description: "Launch Ludork"; Flags: nowait postinstall skipifsilent runasoriginaluser; Check: CanLaunchEditor

[Code]
const
  InstallerKey = 'Software\Ludork\Installer';
  ProjectKey = 'Software\Classes\Ludork.Project';
  PluginWarning = 'Installing official plug-ins replaces the initiating user''s entire ~/Ludork/Plugins folder and plugins.json. This deletes all third-party and project plug-ins, registrations and plug-in data. Back up anything you need. Other user data and template caches are preserved.';

var
  PluginsFailed, PayloadInstalled: Boolean;
  ObsoleteFiles: TStringList;
  UninstallUserSid: String;
  UninstallActionsReady: Boolean;

function PrepareExeUserDataRemoval(Owner: HWND; Silent, RequestedRemoval: Integer; UserSid: String; Capacity: Cardinal): Cardinal;
  external 'PrepareExeUserDataRemoval@{tmp}\LudorkUninstallActions.dll stdcall uninstallonly delayload';

function RemoveExeUserData(UserSid: String): Cardinal;
  external 'RemoveExeUserData@{tmp}\LudorkUninstallActions.dll stdcall uninstallonly delayload';

function InstallerFileAttributes(const FileName: String): Cardinal;
  external 'GetFileAttributesW@kernel32.dll stdcall';

procedure CheckPayloadPath(const FullPath: String);
var
  CurrentPath, RootPath: String;
  Attributes: Cardinal;
begin
  RootPath := RemoveBackslashUnlessRoot(ExpandConstant('{app}'));
  CurrentPath := FullPath;
  while CompareText(CurrentPath, RootPath) <> 0 do begin
    Attributes := InstallerFileAttributes(CurrentPath);
    if (Attributes <> $FFFFFFFF) and ((Attributes and $400) <> 0) then
      RaiseException('An installed payload path is a link or reparse point. Uninstall Ludork before continuing: ' + CurrentPath);
    CurrentPath := ExtractFileDir(CurrentPath);
    if Length(CurrentPath) < Length(RootPath) then
      RaiseException('The installer payload path escapes the installation directory.');
  end;
end;

function ManagedPayloadPath(const RelativePath: String): Boolean;
var
  Path: String;
begin
  Path := Lowercase(RelativePath);
  Result := (Pos('binaries\', Path) = 1) or (Pos('locale\', Path) = 1) or
    (Pos('licenses\', Path) = 1) or (Pos('tools\', Path) = 1) or
    (Path = 'ludork.exe') or (Path = 'ludorkproject.ico') or
    (Path = 'license.md') or (Path = 'readme.md') or (Path = 'readme_zh_cn.md') or
    (Path = 'third_party_notices.md') or (Path = 'third_party_notices_zh_cn.md');
end;

procedure DeinitializeSetup;
var
  Index: Integer;
  FullPath, BackupPath: String;
begin
  if ObsoleteFiles = nil then exit;
  if not PayloadInstalled then begin
    for Index := 0 to ObsoleteFiles.Count - 1 do begin
      FullPath := AddBackslash(ExpandConstant('{app}')) + ObsoleteFiles[Index];
      BackupPath := ExpandConstant('{tmp}\ObsoletePayload\') + ObsoleteFiles[Index];
      ForceDirectories(ExtractFileDir(FullPath));
      if not CopyFile(BackupPath, FullPath, False) then
        SuppressibleMsgBox('Cannot restore an old Ludork file after the interrupted upgrade: ' + FullPath + '. Recover it from ' + BackupPath + ' before closing this message.', mbError, MB_OK, IDOK);
    end;
  end;
  ObsoleteFiles.Free;
end;

function CanLaunchEditor: Boolean;
begin
  Result := not PluginsFailed;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpFinished) and PluginsFailed then begin
    WizardForm.FinishedHeadingLabel.Caption := 'Ludork installed; plug-in installation failed';
    WizardForm.FinishedLabel.Caption := 'Check ~/Ludork/Log/OfficialPluginInstall.log, if created, and the installer log for recovery details. Close Ludork, then run the official plug-in installation command as your normal user.';
  end;
end;

procedure RemoveObsoletePayload;
var
  PreviousFiles, CurrentFiles: TStringList;
  PreviousManifest, RelativePath, FullPath, BackupPath: String;
  Index: Integer;
begin
  PreviousManifest := ExpandConstant('{app}\tools\InstallerPayload.txt');
  if not FileExists(PreviousManifest) then exit;
  CheckPayloadPath(PreviousManifest);
  ObsoleteFiles := TStringList.Create;
  ExtractTemporaryFile('InstallerPayload.txt');
  PreviousFiles := TStringList.Create;
  CurrentFiles := TStringList.Create;
  try
    PreviousFiles.LoadFromFile(PreviousManifest);
    CurrentFiles.LoadFromFile(ExpandConstant('{tmp}\InstallerPayload.txt'));
    CurrentFiles.Sorted := True;
    for Index := 0 to PreviousFiles.Count - 1 do begin
      RelativePath := PreviousFiles[Index];
      if (RelativePath = '') or (Copy(RelativePath, 1, 1) = '\') or
         (Pos('..', RelativePath) > 0) or (Pos(':', RelativePath) > 0) or
         (Pos('/', RelativePath) > 0) or not ManagedPayloadPath(RelativePath) then
        RaiseException('The previous installer payload manifest contains an invalid path. Uninstall Ludork before continuing.');
      if CurrentFiles.IndexOf(RelativePath) < 0 then begin
        FullPath := AddBackslash(ExpandConstant('{app}')) + RelativePath;
        CheckPayloadPath(FullPath);
        if FileExists(FullPath) then begin
          BackupPath := ExpandConstant('{tmp}\ObsoletePayload\') + RelativePath;
          if not ForceDirectories(ExtractFileDir(BackupPath)) or not CopyFile(FullPath, BackupPath, True) then
            RaiseException('Cannot back up an obsolete Ludork file: ' + FullPath);
          if not DeleteFile(FullPath) then
            RaiseException('Cannot remove an obsolete Ludork file: ' + FullPath);
          ObsoleteFiles.Add(RelativePath);
        end;
      end;
    end;
  finally
    CurrentFiles.Free;
    PreviousFiles.Free;
  end;
end;

function LegacyInstallationExists: Boolean;
var
  Names: TArrayOfString;
  Index: Integer;
begin
  Result := RegKeyExists(HKCU, InstallerKey);
  if Result then exit;
  if RegGetSubkeyNames(HKU, '', Names) then begin
    for Index := 0 to GetArrayLength(Names) - 1 do begin
      if RegKeyExists(HKU, Names[Index] + '\' + InstallerKey) then begin
        Result := True;
        exit;
      end;
    end;
  end;
end;

function InitializeSetup: Boolean;
var
  FormatName, InstalledVersion: String;
  PreviousVersion, NewVersion: Int64;
begin
  Result := False;
  if LegacyInstallationExists or
     (RegQueryStringValue(HKLM64, InstallerKey, 'Format', FormatName) and
      (CompareText(FormatName, 'exe') <> 0)) then begin
    MsgBox('Uninstall the existing MSI or per-user Ludork installation before running this EXE installer.', mbError, MB_OK);
    exit;
  end;
  if RegQueryStringValue(HKLM64, InstallerKey, 'Version', InstalledVersion) then begin
    if not StrToVersion(InstalledVersion, PreviousVersion) or
       not StrToVersion('{#ProductVersion}', NewVersion) then begin
      MsgBox('Cannot validate the installed Ludork version. Uninstall it before continuing.', mbError, MB_OK);
      exit;
    end;
    if ComparePackedVersion(PreviousVersion, NewVersion) > 0 then begin
      MsgBox('A newer version of Ludork is already installed.', mbError, MB_OK);
      exit;
    end;
  end;
  Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectTasks) and WizardIsTaskSelected('officialplugins') then
    Result := MsgBox(PluginWarning + #13#10#13#10 + 'Replace and install official plug-ins?', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Locator, Services, Processes: Variant;
  PreviousDirectory: String;
begin
  Result := '';
  if RegQueryStringValue(HKLM64, InstallerKey, 'InstallDir', PreviousDirectory) and
     (CompareText(RemoveBackslashUnlessRoot(PreviousDirectory), RemoveBackslashUnlessRoot(WizardDirValue)) <> 0) then begin
    Result := 'Uninstall Ludork before changing the location of an existing EXE installation.';
    exit;
  end;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Services := Locator.ConnectServer('', 'root\CIMV2');
    Processes := Services.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name = ''Ludork.exe''');
    if Processes.Count > 0 then
      Result := 'Close Ludork before installing the editor or replacing official plug-ins.';
  except
    Result := 'Cannot check whether Ludork is running. Check Windows Management Instrumentation and retry.';
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
  PluginsInstalled: Boolean;
begin
  if CurStep = ssInstall then RemoveObsoletePayload;
  if CurStep = ssPostInstall then PayloadInstalled := True;
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('officialplugins') then begin
    Log(PluginWarning);
    ResultCode := -1;
    try
      PluginsInstalled := ExecAsOriginalUser(ExpandConstant('{app}\tools\ScriptTools\ScriptTools.exe'),
        'editor-official-plugins install-current-user "' + ExpandConstant('{app}\tools\OfficialPlugins') + '"',
        ExpandConstant('{app}'), SW_SHOW, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
    except
      Log(GetExceptionMessage);
      PluginsInstalled := False;
    end;
    if not PluginsInstalled then begin
      PluginsFailed := True;
      SuppressibleMsgBox('Ludork is installed, but official plug-in installation failed (code ' + IntToStr(ResultCode) + '). Check ~/Ludork/Log/OfficialPluginInstall.log, if created, and the installer log for recovery details. Run the plug-in installer as your normal user after closing Ludork.', mbError, MB_OK, IDOK);
    end;
  end;
end;

function GetCustomSetupExitCode: Integer;
begin
  if PluginsFailed then Result := 1 else Result := 0;
end;

procedure DeleteOwnedValue(const Key, Expected: String);
var
  Current: String;
begin
  if RegQueryStringValue(HKLM64, Key, '', Current) and (CompareText(Current, Expected) = 0) then begin
    if not RegDeleteValue(HKLM64, Key, '') then
      RaiseException('Cannot remove the Ludork file association: ' + Key);
    RegDeleteKeyIfEmpty(HKLM64, Key);
  end;
end;

function InitializeUninstall: Boolean;
var
  RemovalOption: String;
  ActionResult: Cardinal;
begin
  Result := False;
  RemovalOption := ExpandConstant('{param:REMOVEUSERDATA|1}');
  if (RemovalOption <> '0') and (RemovalOption <> '1') then begin
    SuppressibleMsgBox('REMOVEUSERDATA must be 0 or 1.', mbError, MB_OK, IDOK);
    exit;
  end;
  if not CopyFile(ExpandConstant('{app}\tools\LudorkUninstallActions.dll'),
                  ExpandConstant('{tmp}\LudorkUninstallActions.dll'), False) then begin
    SuppressibleMsgBox('Cannot prepare the uninstall helper. Repair Ludork and retry uninstalling.', mbError, MB_OK, IDOK);
    exit;
  end;
  SetLength(UninstallUserSid, 256);
  ActionResult := PrepareExeUserDataRemoval(0, Ord(UninstallSilent), StrToInt(RemovalOption),
                                           UninstallUserSid, Length(UninstallUserSid));
  UninstallActionsReady := True;
  UninstallUserSid := Copy(UninstallUserSid, 1, Pos(#0, UninstallUserSid) - 1);
  if ActionResult = 1602 then exit;
  if ActionResult = 50 then begin
    UninstallUserSid := '';
    Log('User data preserved: the initiating user could not be verified.');
    SuppressibleMsgBox('The user who started uninstall could not be verified. All user data will be kept. After uninstall, remove your own ~/Ludork directory manually if desired; do not remove another account''s data.', mbInformation, MB_OK, IDOK);
  end else if ActionResult <> 0 then begin
    SuppressibleMsgBox('Cannot prepare user-data removal (code ' + IntToStr(ActionResult) + '). Close Ludork and other applications using its data, then retry.', mbError, MB_OK, IDOK);
    exit;
  end;
  Result := True;
end;

procedure DeinitializeUninstall;
begin
  if UninstallActionsReady then
    UnloadDLL(ExpandConstant('{tmp}\LudorkUninstallActions.dll'));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if CurUninstallStep = usUninstall then begin
    if RegQueryStringValue(HKLM64, ProjectKey + '\shell\open\command', '', Command) and
       (CompareText(Command, '"' + ExpandConstant('{app}\Ludork.exe') + '" "%1"') = 0) then begin
      DeleteOwnedValue('Software\Classes\.proj', 'Ludork.Project');
      DeleteOwnedValue(ProjectKey + '\shell\open\command', Command);
      DeleteOwnedValue(ProjectKey + '\shell\open', 'Open');
      RegDeleteKeyIfEmpty(HKLM64, ProjectKey + '\shell');
      DeleteOwnedValue(ProjectKey + '\DefaultIcon', '"' + ExpandConstant('{app}\LudorkProject.ico') + '",0');
      DeleteOwnedValue(ProjectKey, 'Ludork Project');
    end;
  end;
  if (CurUninstallStep = usPostUninstall) and (UninstallUserSid <> '') then begin
    if RemoveExeUserData(UninstallUserSid) <> 0 then
      RaiseException('The editor was uninstalled, but user data could not be completely removed. Close applications using ~/Ludork and remove the remaining directory manually.');
  end;
end;
