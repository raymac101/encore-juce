; Encore Karaoke - Inno Setup installer script
;
; Built by CI (.github/workflows/release.yml) via:
;   iscc installer.iss /DMyAppVersion=1.1.0 /DSourceExe="path\to\Encore Karaoke.exe"
;
; MyAppVersion/SourceExe are passed in rather than hardcoded so the same
; script works for every release without editing this file each time.
;
; CloseApplications/AppMutex are a safety net only -- Encore's own
; "Restart Now" update-banner flow (Source/Services/UpdateService.cpp)
; always fully quits the app before ever launching this installer, so this
; exists purely to cover a customer manually re-running the installer while
; Encore happens to still be open.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\..\build\EncoreJUCE_artefacts\Release\Encore Karaoke.exe"
#endif

#define MyAppName "Encore Karaoke"
#define MyAppPublisher "Viracicom"
#define MyAppExeName "Encore Karaoke.exe"

[Setup]
AppId={{B6E1C9C0-6E6E-4A1E-9E5C-3E1E1E1E1E1E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=EncoreKaraoke-{#MyAppVersion}-win64
OutputDir=..\..\dist
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\{#MyAppExeName}
; No SetupIconFile yet -- only a .png app icon exists (see CMakeLists.txt
; ICON_BIG/ICON_SMALL). Convert assets/images/Encore-Icon.png to a .ico and
; point SetupIconFile at it here if a branded installer icon is wanted; the
; installed app's shortcuts already show the .exe's own embedded icon
; regardless.
; Safety net only (see header comment) -- Restart-Manager-based detection
; of a running "Encore Karaoke.exe" holding the target file open, so the
; installer can close it before overwriting. Encore allows multiple
; instances (moreThanOneInstanceAllowed() in Main.cpp) and defines no named
; mutex, so this relies on Restart Manager's file-handle detection rather
; than AppMutex.
CloseApplications=yes
CloseApplicationsFilter=Encore Karaoke.exe
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
