param(
    [string]$GameDir = '',
    [string]$BuildDir = '',
    [string]$AnalysisDir = ''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(-not $GameDir){$GameDir=Join-Path $projectRoot 'original/extracted'}
if(-not $BuildDir){$BuildDir=Join-Path $projectRoot 'build/native'}
if(-not $AnalysisDir){$AnalysisDir=Join-Path $projectRoot 'analysis'}
New-Item -ItemType Directory -Force -Path $AnalysisDir | Out-Null
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$cmake=Get-DisneyCMake
Write-DisneyProgress 5 'Configurando e compilando os inicializadores IOP...'
& $cmake -S $projectRoot -B $BuildDir -G 'Visual Studio 17 2022' -A x64 '-DDISNEY_BUILD_LEGACY_PROTOTYPE=OFF' '-DDISNEY_BUILD_ORIGINAL_AOT_CANDIDATE=OFF' '-DDISNEY_BUILD_RESOURCE_LAB=OFF' '-DDISNEY_BUILD_ORIGINAL_IOP=ON'
if($LASTEXITCODE -ne 0){throw 'Falha na configuracao nativa'}
& $cmake --build $BuildDir --config Release --target DisneyOriginalIopBootTest --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha na compilacao dos modulos originais'}
Write-DisneyProgress 60 'Validando os inicializadores IOP com os modulos extraidos...'
& "$projectRoot\dist\native-tools\Release\DisneyOriginalIopBootTest.exe" (Join-Path $GameDir 'IOP') 2>&1 | Tee-Object -FilePath (Join-Path $AnalysisDir 'native-iop-boot-test.log')
if($LASTEXITCODE -ne 0){throw 'Inicializacao nativa original incompleta; veja o log'}
Write-DisneyProgress 100 'Inicializadores IOP validados.'
Write-Output 'Inicializadores IOP validados. O jogo completo ainda nao e jogavel.'
