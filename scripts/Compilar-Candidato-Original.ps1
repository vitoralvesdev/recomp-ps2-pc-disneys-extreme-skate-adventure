param([switch]$SomenteRuntime)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$cmake="$projectRoot\tools\cmake\cmake-4.4.4-windows-x86_64\bin\cmake.exe"
& "$PSScriptRoot\Testar-Inicializacao-Original.ps1"
& $cmake -S $projectRoot -B "$projectRoot\build\port" '-DDISNEY_BUILD_ORIGINAL_AOT_CANDIDATE=ON' '-DDISNEY_BUILD_LEGACY_PROTOTYPE=OFF' '-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
if($LASTEXITCODE -ne 0){throw 'Falha ao configurar candidato original'}
$buildTargets=if($SomenteRuntime){@('ps2_runtime')}else{@('ps2EntryRunner','DisneySkateLauncher')}
& $cmake --build "$projectRoot\build\port" --config Release --target @buildTargets --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar candidato original'}
Write-Output "Compilado. Abra: $projectRoot\Disney's Extreme Skate Adventure.exe"
