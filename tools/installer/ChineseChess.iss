; 仅安装已构建程序和说明，不包含个人经验、棋局或教师模型。
#ifndef GameExe
  #error 请通过 /DGameExe 指定 Release x64 程序路径
#endif
#ifndef PackageVersion
  #define PackageVersion "0.1.0-rc.1"
#endif

[Setup]
AppId={{C4159C4A-7F26-47B0-9EAF-316B8D5756A7}
AppName=中国象棋
AppVersion={#PackageVersion}
AppPublisher=Gnt191612
AppPublisherURL=https://github.com/Gnt191612/Chinese-Chess
DefaultDirName={localappdata}\Programs\ChineseChess
DefaultGroupName=中国象棋
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\..\build\installer
OutputBaseFilename=中国象棋-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\中国象棋.exe
CloseApplications=yes
RestartApplications=no
DisableProgramGroupPage=yes
UsePreviousAppDir=yes

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："

[Files]
Source: "{#GameExe}"; DestDir: "{app}"; DestName: "中国象棋.exe"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\docs\PORTABLE_PACKAGE.md"; DestDir: "{app}"; DestName: "安装版说明.md"; Flags: ignoreversion

[Icons]
Name: "{group}\中国象棋"; Filename: "{app}\中国象棋.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\中国象棋"; Filename: "{app}\中国象棋.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{group}\卸载中国象棋"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\中国象棋.exe"; Description: "启动中国象棋"; Flags: nowait postinstall skipifsilent

; 不使用递归卸载删除，保留程序运行生成的经验和棋局。
