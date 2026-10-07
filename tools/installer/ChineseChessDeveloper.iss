; 开发者安装版隔离提供官方教师、匹配权重、许可和对应源码；不重打包VS或EasyX。
#ifndef SourceRoot
  #error 请指定由ExportSource生成的干净源码目录
#endif
#ifndef TeacherRoot
  #error 请指定已校验且包含许可与对应源码的固定教师目录
#endif
[Setup]
AppId={{6DB2AFCA-D2CF-4357-81B1-58025D8B16A0}
AppName=中国象棋开发工作区
AppVersion=0.2.0-dev.2
AppPublisher=Gnt191612
DefaultDirName={localappdata}\ChineseChessDev
PrivilegesRequired=lowest
DisableProgramGroupPage=yes
OutputDir=..\..\build\developer-installer
OutputBaseFilename=Chinese-Chess-Developer-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
MinVersion=10.0

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Files]
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
#ifdef TeacherRoot
Source: "{#TeacherRoot}\*"; DestDir: "{app}\training\engines\pikafish-2026-09-06"; Flags: recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
#endif

[Icons]
Name: "{group}\配置开发环境"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoExit -ExecutionPolicy Bypass -File ""{app}\tools\SetupDeveloper.ps1"""; WorkingDir: "{app}"
Name: "{group}\中国象棋源码目录"; Filename: "{app}"
Name: "{group}\配置皮卡鱼训练环境"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoExit -ExecutionPolicy Bypass -File ""{app}\tools\SetupTraining.ps1"""; WorkingDir: "{app}"

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoExit -ExecutionPolicy Bypass -File ""{app}\tools\SetupDeveloper.ps1"""; Description: "启动开发环境配置助手（联网，第三方安装需确认）"; Flags: nowait postinstall skipifsilent

; 脚本策略参数仅作用于该进程；不修改系统策略，不覆盖开发者修改。
