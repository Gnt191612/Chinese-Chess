param([Parameter(Mandatory = $true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$target = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $target) { throw '目标目录已存在，请使用新的目录，避免覆盖已有文件。' }
$zip = $target + '.zip'
if (Test-Path -LiteralPath $zip) { throw '目标压缩包已存在，请使用新的目录名。' }
$paths = @(& git -c "safe.directory=$($projectRoot.Replace('\','/'))" -c core.quotepath=false -C $projectRoot ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw '无法读取源码文件清单。' }
$ignored = @($paths | & git -c "safe.directory=$($projectRoot.Replace('\','/'))" -c core.quotepath=false -C $projectRoot check-ignore --no-index --stdin)
if ($LASTEXITCODE -gt 1) { throw '无法校验忽略规则。' }
$paths = @($paths | Sort-Object -Unique | Where-Object {
    $_ -notin $ignored -and ($_ -match '^(src|tests|tools|training|docs|community-experience|\.github)/' -or
        $_ -match '^([^/]+\.md|LICENSE|中国象棋\.(sln|vcxproj|vcxproj\.filters)|\.gitignore|\.gitattributes)$')
})
if ($paths.Count -eq 0) { throw '源码清单为空。' }
New-Item -ItemType Directory -Path $target | Out-Null
foreach ($relative in $paths) {
    $source = Join-Path $projectRoot $relative
    $destination = Join-Path $target $relative
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "文件不存在：$relative" }
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
}
$manifest = foreach ($relative in $paths) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $target $relative) -Algorithm SHA256).Hash
    "$hash  $relative"
}
[IO.File]::WriteAllLines((Join-Path $target 'SOURCE_SHA256.txt'), $manifest, [Text.UTF8Encoding]::new($false))
Compress-Archive -LiteralPath $target -DestinationPath $zip
Write-Output "已导出 $($paths.Count) 个源码与配置文件：$target"
Write-Output "源码压缩包：$zip"
