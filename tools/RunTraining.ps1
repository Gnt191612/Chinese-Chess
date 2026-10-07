param(
    [Parameter(Mandatory = $true)][string]$PgnPath,
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9_-]{1,64}$')][string]$RunName
)
$ErrorActionPreference = 'Stop'
$runRoot = Split-Path $PSScriptRoot -Parent
$pgnInput = (Resolve-Path -LiteralPath $PgnPath).Path
$runOutput = Join-Path $runRoot "training\models\$RunName"
if (Test-Path -LiteralPath $runOutput) { throw '批次目录已存在，不覆盖旧训练。' }
Push-Location $runRoot
try {
    & python.exe tools/training/setup_teacher.py --check-only
    if ($LASTEXITCODE -ne 0) { throw '先运行SetupTraining.ps1准备教师。' }
    if (!(Test-Path -LiteralPath 'build\training-probe\PositionProbe.exe')) { throw '缺少棋谱校验器，请运行SetupTraining.ps1。' }
    New-Item -ItemType Directory -Path $runOutput | Out-Null
    $positions = Join-Path $runOutput 'positions.jsonl'
    $labels = Join-Path $runOutput 'labels.jsonl'
    & python.exe tools/training/convert_pgn.py --input $pgnInput --probe build/training-probe/PositionProbe.exe --output $positions
    if ($LASTEXITCODE -ne 0) { throw '转换失败，未启动教师。' }
    & python.exe tools/training/annotate_uci.py --config training/config/local.json --input $positions --output $labels
    if ($LASTEXITCODE -ne 0) { throw '标注失败，保留批次供诊断。' }
    & python.exe tools/training/validate_labels.py --input $labels
    if ($LASTEXITCODE -ne 0) { throw '标签验证失败。' }
    & python.exe tools/training/fit_light.py --input $labels --output (Join-Path $runOutput 'fit')
    if ($LASTEXITCODE -ne 0) { throw '拟合失败；检查是否包含合格训练局面。' }
    Write-Host "参数和报告保存在：$runOutput；未自动替换游戏模型，须另行对照测试。"
} finally { Pop-Location }
