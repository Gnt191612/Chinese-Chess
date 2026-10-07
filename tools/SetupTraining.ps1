param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$trainingRoot = Split-Path $PSScriptRoot -Parent
Push-Location $trainingRoot
try {
    & .\tools\SetupDeveloper.ps1 -CheckOnly
    if ($LASTEXITCODE -ne 0) { throw '开发环境验证失败。' }
    $teacherArguments = @('tools/training/setup_teacher.py', '--repo', $trainingRoot)
    if ($CheckOnly) { $teacherArguments += '--check-only' }
    & python.exe @teacherArguments
    if ($LASTEXITCODE -ne 0) { throw '教师文件配置失败。' }
    & .\tools\training\build_probe.bat
    if ($LASTEXITCODE -ne 0) { throw '棋谱校验器构建失败。' }
    & python.exe tools/training/preflight.py --repo . --config training/config/local.json
    if ($LASTEXITCODE -ne 0) { throw '训练预检失败。' }
    if (!$CheckOnly -and !(Test-Path -LiteralPath 'training\data\raw\ccpd-smoke')) {
        & python.exe tools/training/prepare_dataset.py
        if ($LASTEXITCODE -ne 0) { throw '样本准备失败。' }
    }
    Write-Host '训练环境已准备；使用tools/RunTraining.ps1显式启动，安装不会自动训练。'
} finally { Pop-Location }
