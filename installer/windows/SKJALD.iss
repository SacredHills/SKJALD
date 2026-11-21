; Inno Setup script. Build: iscc /DAppVersion=1.0.0 /DSourceDir=<package dir> /DOutDir=<out dir> SKJALD.iss
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\package"
#endif
#ifndef OutDir
  #define OutDir "..\.."
#endif

[Setup]
AppId={{6B7C3E52-4A1D-4C8E-9B70-5A4B0D1E2F31}
AppName=SKJALD
AppVersion={#AppVersion}
AppPublisher=Sacred Hills Studio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commonappdata}\SKJALD
OutputDir={#OutDir}
OutputBaseFilename=SKJALD-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Files]
; VST3 bundle -> C:\Program Files\Common Files\VST3\SKJALD.vst3
Source: "{#SourceDir}\SKJALD.vst3\*"; DestDir: "{commoncf64}\VST3\SKJALD.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
; Offline processor and docs -> C:\Program Files\SKJALD
Source: "{#SourceDir}\skjald_offline.exe"; DestDir: "{commonpf64}\SKJALD"; Flags: ignoreversion
Source: "{#SourceDir}\INSTALL.md"; DestDir: "{commonpf64}\SKJALD"; Flags: ignoreversion

[Messages]
FinishedLabel=SKJALD was installed to the VST3 folder. Rescan plug-ins in your DAW (Sacred Hills Studio > SKJALD).
