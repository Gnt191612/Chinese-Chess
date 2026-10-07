param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$developerRoot = Split-Path $PSScriptRoot -Parent
$downloadRoot = Join-Path $developerRoot 'build\developer-downloads'
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
function Find-CppTools {
    if (Test-Path -LiteralPath $vswherePath) {
        & $vswherePath -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
}
function Confirm-Install([string]$description) {
    Write-Host $description
    if ((Read-Host '输入 YES 继续；其他输入取消') -cne 'YES') { throw '用户取消环境配置。' }
}
$visualStudioPath = Find-CppTools
if (!$visualStudioPath) {
    if ($CheckOnly) { throw '缺少VS2022 C++工具，请运行配置助手安装。' }
    Confirm-Install '将调用winget从官方渠道安装VS2022 Build Tools的C++组件，下载可能需要数GB；许可与管理员提示由你确认。'
    & winget.exe install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --interactive --override '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --wait --norestart'
    if ($LASTEXITCODE -eq 3010) { throw '安装要求重启，请重启后重新运行助手。' }
    $visualStudioPath = Find-CppTools
    if (!$visualStudioPath) { throw 'C++工具安装未完成。' }
}
$easyxHeader = Join-Path $visualStudioPath 'VC\Auxiliary\VS\include\graphics.h'
$easyxLibrary = Join-Path $visualStudioPath 'VC\Auxiliary\VS\lib\x64\EasyXa.lib'
if (!(Test-Path -LiteralPath $easyxHeader) -or !(Test-Path -LiteralPath $easyxLibrary)) {
    if ($CheckOnly) { throw '缺少EasyX x64开发依赖。' }
    Confirm-Install '将下载并运行EasyX官方安装向导，请选择刚安装的VS2022；不会自动绕过许可或安全提示。'
    New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
    $easyxInstaller = Join-Path $downloadRoot 'EasyX_26.9.25.exe'
    & curl.exe --fail -L --noproxy '*' -o $easyxInstaller 'https://easyx.cn/download/EasyX_26.9.25.exe'
    if ($LASTEXITCODE -ne 0) { throw 'EasyX官方下载失败，未启动安装程序。' }
    if ((Get-Item -LiteralPath $easyxInstaller).Length -lt 65536) { throw '下载文件过小，未启动安装程序。' }
    $easyxFile = [IO.File]::OpenRead($easyxInstaller)
    try { if ($easyxFile.ReadByte() -ne 77 -or $easyxFile.ReadByte() -ne 90) { throw '下载内容不是EXE。' } } finally { $easyxFile.Dispose() }
    Get-FileHash -LiteralPath $easyxInstaller -Algorithm SHA256
    $easyxProcess = Start-Process -FilePath $easyxInstaller -Wait -PassThru
    if (!(Test-Path -LiteralPath $easyxHeader) -or !(Test-Path -LiteralPath $easyxLibrary)) { throw 'EasyX安装未完成。' }
}
$pythonReady = $false
if (Get-Command python.exe -ErrorAction SilentlyContinue) {
    & python.exe -c 'import sys; sys.exit(0 if sys.version_info >= (3,11) else 1)'
    $pythonReady = $LASTEXITCODE -eq 0
}
if (!$pythonReady) {
    if ($CheckOnly) { throw '训练工具需要Python 3.11或更高版本。' }
    Confirm-Install '将从winget官方渠道安装Python 3.11。'
    & winget.exe install --id Python.Python.3.11 --exact --source winget --interactive
    throw 'Python安装后请重新打开配置助手，以刷新PATH并完成验证。'
}
$msbuildPath = Join-Path $visualStudioPath 'MSBuild\Current\Bin\MSBuild.exe'
Push-Location $developerRoot
try {
    & $msbuildPath '中国象棋.sln' /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw '项目构建失败，环境不能标记为就绪。' }
    & python.exe tests/test_community_experience.py
    if ($LASTEXITCODE -ne 0) { throw '社区工具测试失败。' }
    & python.exe tests/test_light_training.py
    if ($LASTEXITCODE -ne 0) { throw '训练工具测试失败。' }
    & python.exe tools/training/test_tools.py
    if ($LASTEXITCODE -ne 0) { throw 'UCI工具测试失败。' }
    & python.exe tests/test_training_setup.py
    if ($LASTEXITCODE -ne 0) { throw '训练配置工具测试失败。' }
    Write-Host "本机开发环境验证完成，源码目录：$developerRoot"
} finally { Pop-Location }
