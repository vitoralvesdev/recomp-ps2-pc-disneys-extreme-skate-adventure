param(
    [string]$Root = '',
    [string]$GameDir = '',
    [string]$BuildDir = '',
    [string]$ToolsDir = '',
    [string]$Python = ''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(-not $Root){$Root=$projectRoot}
if(-not $GameDir){$GameDir=Join-Path $Root 'original/extracted'}
if(-not $BuildDir){$BuildDir=Join-Path $Root 'build/music-recovery'}
if(-not $ToolsDir){$ToolsDir=Join-Path $Root 'tools'}
$Root=[IO.Path]::GetFullPath($Root)
$GameDir=[IO.Path]::GetFullPath($GameDir)
$BuildDir=[IO.Path]::GetFullPath($BuildDir)
$ToolsDir=[IO.Path]::GetFullPath($ToolsDir)
$source=Join-Path $PSScriptRoot 'music-recovery'
$output=Join-Path $BuildDir 'output'
$recover=Join-Path $ToolsDir 'music-recovery/DisneyMusicRecover.exe'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
if(Test-Path -LiteralPath $recover -PathType Leaf){
    Write-DisneyProgress 5 'Usando o conversor offline de musica incluido...'
} else {
    $cmake=Get-DisneyCMake
    Write-DisneyProgress 5 'Compilando o conversor offline de musica...'
    & $cmake -S $source -B $BuildDir -G 'Visual Studio 17 2022' -A x64
    if($LASTEXITCODE -ne 0){throw 'Falha ao configurar conversor offline'}
    & $cmake --build $BuildDir --config Release --parallel 2
    if($LASTEXITCODE -ne 0){throw 'Falha ao compilar conversor offline'}
    $recover=Join-Path $BuildDir 'Release/DisneyMusicRecover.exe'
}
Write-DisneyProgress 35 'Convertendo MUSIC.WAD extraido da ISO...'
$oldGameDir=$env:DISNEY_GAME_DIR
try {
    $env:DISNEY_GAME_DIR=$GameDir
    & $recover $Root $output | Set-Content -LiteralPath (Join-Path $output 'decoded.tsv') -Encoding utf8
    if($LASTEXITCODE -ne 0){throw 'Falha ao converter MUSIC.WAD'}
} finally {$env:DISNEY_GAME_DIR=$oldGameDir}
if(-not $Python){$Python=Join-Path $ToolsDir 'python/python.exe'}
if(-not (Test-Path -LiteralPath $Python -PathType Leaf)){$Python='python'}
Write-DisneyProgress 75 'Validando e instalando as trilhas convertidas...'
& $Python (Join-Path $source 'install_music.py') $Root $output
if($LASTEXITCODE -ne 0){throw 'Falha ao validar/instalar musicas'}
Write-DisneyProgress 100 'Trilhas recuperadas e verificadas.'
