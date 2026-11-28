param(
    [string]$OutputZip = '',
    [string]$RuntimeDirectory = '',
    [string]$RedistDirectory = '',
    [string]$NoticesDirectory = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'Package.Common.psm1')
if (-not $RuntimeDirectory) { $RuntimeDirectory = Join-Path $projectRoot 'runtime' }
if (-not $OutputZip) {
    $version = Get-DisneyPackageVersion $projectRoot
    $OutputZip = Join-Path $projectRoot "dist/DisneySkateRuntime-$version.zip"
}
$redist = Get-DisneyRedistDirectory $projectRoot $RedistDirectory
$work = Join-Path $projectRoot ('build/runtime-package/' + [Guid]::NewGuid().ToString('N'))
$payload = Join-Path $work 'payload'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputZip), $payload | Out-Null
try {
    Copy-DisneyRuntime $RuntimeDirectory $payload $redist
    foreach ($file in Get-ChildItem -LiteralPath $payload -File) {
        if (-not (Test-DisneyPeX64 $file.FullName)) { throw "PE x64 invalido: $($file.Name)" }
    }
    if ($NoticesDirectory) {
        if (-not (Test-Path -LiteralPath $NoticesDirectory -PathType Container)) {
            throw "Diretorio de notices ausente: $NoticesDirectory"
        }
        Copy-Item -Path (Join-Path $NoticesDirectory '*') -Destination (Join-Path $payload 'notices') -Recurse -Force
    }
    $source = Get-DisneySourceState $projectRoot
    $version = $source.launcher_version
    $lines = @(
        "source_commit=$($source.source_commit)",
        "source_dirty=$($source.source_dirty)",
        "source_state_sha256=$($source.source_state_sha256)",
        "launcher_version=$version", 'format=1',
        'runtime_sha256=' + (Get-DisneyFileHash (Join-Path $payload 'DisneySkateRuntime.exe'))
    )
    [IO.File]::WriteAllLines((Join-Path $payload 'BUILD-INFO.txt'), $lines, (New-Object Text.UTF8Encoding($false)))
    Compress-DisneyPortable $payload $OutputZip $version
    $hash = Get-DisneyFileHash $OutputZip
    [IO.File]::WriteAllText("$OutputZip.sha256", "$hash  $([IO.Path]::GetFileName($OutputZip))`r`n",
        (New-Object Text.UTF8Encoding($false)))
    Write-Output "Runtime empacotado: $OutputZip"
    Write-Output "SHA-256: $hash"
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
