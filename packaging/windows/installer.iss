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
; The build output directory (contains the exe plus its runtime DLLs,
; Resources/Languages, and assets) -- derived from SourceExe so callers only
; need to pass one path in, same as before.
#define SourceDir ExtractFilePath(SourceExe)
; Microsoft Visual C++ 2015-2022 Redistributable (x64). Encore and its vcpkg
; DLLs (rubberband, sqlite3) link the MSVC runtime dynamically, so
; MSVCP140.dll / VCRUNTIME140.dll / VCRUNTIME140_1.dll must exist on the
; target machine -- a fresh Windows install doesn't have them. CI downloads
; this next to the script before running iscc (see release.yml); for a
; local build fetch it from https://aka.ms/vs/17/release/vc_redist.x64.exe.
; Deliberately not optional: without it the installer would ship a build
; that fails to launch on clean PCs.
#define VCRedistExe "vc_redist.x64.exe"

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
; Ship the whole build output directory -- the exe alone is not enough to
; run; RubberBand/SQLite/etc. DLLs, Resources/Languages, and assets all sit
; alongside it and are loaded relative to the exe at runtime.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; VC++ runtime installer -- extracted to a temp dir, run from [Run] only if
; needed, then deleted.
Source: "{#VCRedistExe}"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"

[Run]
; Must run before the launch entry below. /quiet: no UI; /norestart: the
; runtime DLLs are usable immediately for a newly-started process.
Filename: "{tmp}\{#VCRedistExe}"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ runtime..."; Check: VCRedistNeedsInstall; Flags: waituntilterminated
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
// True when the machine's VC++ 14.x x64 runtime is missing or older than
// the bundled redist. Matters beyond "missing": an app built with a newer
// MSVC toolset can crash on startup against an older MSVCP140.dll, so a
// stale runtime gets upgraded too. Already-current machines (and every
// auto-update after the first install) skip the redist entirely.
function VCRedistNeedsInstall: Boolean;
var
  Installed, Major, Minor, Bld: Cardinal;
  BundledVer: String;
  BMajor, BMinor, BBld: Cardinal;
  P: Integer;
  Key: String;
begin
  Result := True;
  Key := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  if not RegQueryDWordValue(HKLM64, Key, 'Installed', Installed) or (Installed <> 1) then
    Exit;
  if not (RegQueryDWordValue(HKLM64, Key, 'Major', Major) and
          RegQueryDWordValue(HKLM64, Key, 'Minor', Minor) and
          RegQueryDWordValue(HKLM64, Key, 'Bld', Bld)) then
    Exit;

  // Bundled redist's file version, e.g. "14.42.34433.0".
  if not GetVersionNumbersString(ExpandConstant('{tmp}\{#VCRedistExe}'), BundledVer) then
  begin
    Result := False;  // can't tell -- an installed 14.x runtime is the best we know
    Exit;
  end;
  P := Pos('.', BundledVer); BMajor := StrToIntDef(Copy(BundledVer, 1, P - 1), 0); Delete(BundledVer, 1, P);
  P := Pos('.', BundledVer); BMinor := StrToIntDef(Copy(BundledVer, 1, P - 1), 0); Delete(BundledVer, 1, P);
  P := Pos('.', BundledVer); BBld   := StrToIntDef(Copy(BundledVer, 1, P - 1), 0);

  if Major <> BMajor then Result := Major < BMajor
  else if Minor <> BMinor then Result := Minor < BMinor
  else Result := Bld < BBld;
end;
