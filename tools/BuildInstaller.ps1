param(
    [Parameter(Mandatory = $true)][string]$GameExe,
    [string]$Compiler = 'D:\Software\Inno Setup 7\ISCC.exe',
    [string]$Version = '0.1.0-rc.1'
)
$ErrorActionPreference = 'Stop'
$programPath = (Resolve-Path -LiteralPath $GameExe).Path
if (!(Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw '未找到 Inno Setup 编译器。' }
if ($Version -notmatch '^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?$') { throw '版本格式无效。' }
$outputPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'build\installer\中国象棋-Setup.exe'
if (Test-Path -LiteralPath $outputPath) { throw '安装包已存在，请先保留旧包并使用新的输出目录。' }
& $Compiler "/DGameExe=$programPath" "/DPackageVersion=$Version" (Join-Path $PSScriptRoot 'installer\ChineseChess.iss')
if ($LASTEXITCODE -ne 0) { throw '安装包编译失败。' }
Get-FileHash -LiteralPath $outputPath -Algorithm SHA256
