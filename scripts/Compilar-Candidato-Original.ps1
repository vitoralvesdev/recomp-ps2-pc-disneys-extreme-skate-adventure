param(
    [switch]$SomenteRuntime,
    [string]$GameDir = '',
    [string]$BuildDir = '',
    [string]$RuntimeOutputDir = '',
    [string]$AnalysisDir = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if(-not $GameDir){$GameDir=Join-Path $projectRoot 'original/extracted'}
if(-not $BuildDir){$BuildDir=Join-Path $projectRoot 'build'}
if(-not $RuntimeOutputDir){$RuntimeOutputDir=Join-Path $projectRoot 'runtime'}
if(-not $AnalysisDir){$AnalysisDir=Join-Path $projectRoot 'analysis'}
New-Item -ItemType Directory -Force -Path $RuntimeOutputDir, $AnalysisDir | Out-Null
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$cmake = Get-DisneyCMake
$nativeBuild = Join-Path $BuildDir 'native'
$portBuild = Join-Path $BuildDir 'port'
$guestBatches = Join-Path $BuildDir 'guest-batches'
$iopLibrary = Join-Path $nativeBuild 'native/original/iop/Release/DisneyOriginalIop.lib'
Write-DisneyProgress 3 'Preparando o PS2Recomp fixado...'
& (Join-Path $PSScriptRoot 'Bootstrap-PS2Recomp.ps1')
if($LASTEXITCODE -and $LASTEXITCODE -ne 0){throw 'Falha ao preparar PS2Recomp'}
Write-DisneyProgress 8 'Validando os modulos IOP extraidos...'
& (Join-Path $PSScriptRoot 'Testar-Inicializacao-Original.ps1') -GameDir $GameDir -BuildDir $nativeBuild -AnalysisDir $AnalysisDir
if(-not (Test-Path -LiteralPath $iopLibrary -PathType Leaf)){throw "Biblioteca IOP nao produzida: $iopLibrary"}
Write-DisneyProgress 25 'Configurando o runtime AOT...'
$configureArguments = @(
    '-S', $projectRoot
    '-B', $portBuild
    '-DDISNEY_BUILD_ORIGINAL_AOT_CANDIDATE=ON'
    '-DDISNEY_BUILD_LEGACY_PROTOTYPE=OFF'
    "-DFETCHCONTENT_BASE_DIR=$(Join-Path $BuildDir 'deps')"
    '-DFETCHCONTENT_FULLY_DISCONNECTED=OFF'
    "-DDISNEY_ORIGINAL_IOP_LIBRARY=$iopLibrary"
    "-DDISNEY_GUEST_BATCH_DIR=$guestBatches"
    "-DDISNEY_RUNTIME_OUTPUT_DIR=$RuntimeOutputDir"
)
& $cmake @configureArguments
if($LASTEXITCODE -ne 0){throw 'Falha ao configurar candidato original'}
$buildTargets=@('ps2EntryRunner')
if(-not $SomenteRuntime){$buildTargets+='DisneySkateLauncher'}
Write-DisneyProgress 35 'Compilando funcoes, VU, IOP e runtime; as DLLs de midia serao copiadas pelo CMake...'
& $cmake --build $portBuild --config Release --target @buildTargets --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar candidato original'}
$runtime=Join-Path $RuntimeOutputDir 'DisneySkateRuntime.exe'
if(-not (Test-Path -LiteralPath $runtime -PathType Leaf)){throw "Runtime nao produzido: $runtime"}
foreach($dll in @('avcodec-61.dll','avformat-61.dll','avutil-59.dll','swresample-5.dll','swscale-8.dll')){
    if(-not (Test-Path -LiteralPath (Join-Path $RuntimeOutputDir $dll) -PathType Leaf)){throw "DLL de midia ausente apos o build: $dll"}
}
Write-DisneyProgress 100 'Runtime e DLLs de midia prontos.'
Write-Output "Compilado. Abra: $runtime"
