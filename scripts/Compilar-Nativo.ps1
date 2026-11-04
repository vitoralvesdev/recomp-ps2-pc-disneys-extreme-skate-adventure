$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$cmake="$projectRoot\tools\cmake\cmake-4.4.4-windows-x86_64\bin\cmake.exe"
& $cmake -S $projectRoot -B "$projectRoot\build\native" -G 'Visual Studio 17 2022' -A x64 '-DDISNEY_BUILD_LEGACY_PROTOTYPE=OFF' '-DDISNEY_BUILD_ORIGINAL_AOT_CANDIDATE=OFF' '-DDISNEY_BUILD_RESOURCE_LAB=OFF'
if($LASTEXITCODE -ne 0){throw 'Falha ao configurar ferramenta nativa'}
& $cmake --build "$projectRoot\build\native" --config Release --target DisneyAssetConverter DisneyOriginalCoreTest --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar conversor'}
& "$projectRoot\dist\native-tools\Release\DisneyOriginalCoreTest.exe"
if($LASTEXITCODE -ne 0){throw 'Teste do nucleo original falhou'}
Write-Output 'Ferramenta e rotina original parcial compiladas. O jogo original nativo ainda nao esta pronto.'