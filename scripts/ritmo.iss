; Inno Setup script of the Windows installer of RITMO (https://jrsoftware.org/isinfo.php).
;
; The workflow .github/workflows/build-windows.yml compiles it after the package folder is made:
;
;   ISCC.exe /DAppVersion=2.3 /DSourceDir=dist\Ritmo /O. scripts\ritmo.iss
;
; AppVersion is the version of the program, SourceDir the folder with Ritmo.exe, its resources and
; its DLLs. The result is Ritmo-Windows-x64-Setup.exe in the folder of /O (the workflow adds the
; version with /FRitmo-Windows-x64-Setup-<version>).

#ifndef AppVersion
  #define AppVersion "0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\Ritmo"
#endif

[Setup]
; the identity of the program for the installer: never change it, it finds the earlier installation
AppId={{C90363AC-E590-44A0-B0DE-152C95BDF5FE}
AppName=RITMO
AppVersion={#AppVersion}
AppVerName=RITMO {#AppVersion}
AppPublisher=RITMO contributors
AppPublisherURL=https://github.com/gianlucarenzi/RITMO-Music-Tracker
AppSupportURL=https://github.com/gianlucarenzi/RITMO-Music-Tracker/issues
AppUpdatesURL=https://github.com/gianlucarenzi/RITMO-Music-Tracker/releases
VersionInfoVersion=1.0.0.0
VersionInfoDescription=RITMO setup
DefaultDirName={autopf}\RITMO
DefaultGroupName=RITMO
DisableProgramGroupPage=yes
; for all the users with an administrator, or for the current user only (the dialog asks)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
LicenseFile=..\LICENSE
SetupIconFile=..\src\res\ritmo.ico
UninstallDisplayIcon={app}\Ritmo.exe
OutputBaseFilename=Ritmo-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; the same files RMT opens: an RMT module, a work file
Name: "assoc"; Description: "Open the .rmt and .rmw files with RITMO"; GroupDescription: "File types:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\RITMO"; Filename: "{app}\Ritmo.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\RITMO"; Filename: "{app}\Ritmo.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.rmt"; ValueType: string; ValueData: "RITMO.Module"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.rmw"; ValueType: string; ValueData: "RITMO.Module"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\RITMO.Module"; ValueType: string; ValueData: "RITMO song"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\RITMO.Module\DefaultIcon"; ValueType: string; ValueData: "{app}\Ritmo.exe,0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\RITMO.Module\shell\open\command"; ValueType: string; ValueData: """{app}\Ritmo.exe"" ""%1"""; Tasks: assoc

[Run]
Filename: "{app}\Ritmo.exe"; Description: "{cm:LaunchProgram,RITMO}"; Flags: nowait postinstall skipifsilent
