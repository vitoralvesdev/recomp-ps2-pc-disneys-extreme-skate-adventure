$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$cmake=Get-DisneyCMake
& $cmake -S $projectRoot -B "$projectRoot\build\native" -G 'Visual Studio 17 2022' -A x64 '-DDISNEY_BUILD_LEGACY_PROTOTYPE=OFF' '-DDISNEY_BUILD_ORIGINAL_AOT_CANDIDATE=OFF' '-DDISNEY_BUILD_RESOURCE_LAB=OFF' '-DDISNEY_BUILD_ORIGINAL_IOP=ON'
if($LASTEXITCODE -ne 0){throw 'Falha na configuracao nativa'}
& $cmake --build "$projectRoot\build\native" --config Release --target DisneyOriginalIopBootTest --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha na compilacao dos modulos originais'}
& "$projectRoot\dist\native-tools\Release\DisneyOriginalIopBootTest.exe" "$projectRoot\original\extracted\IOP" 2>&1 | Tee-Object -FilePath "$projectRoot\analysis\native-iop-boot-test.log"
if($LASTEXITCODE -ne 0){throw 'Inicializacao nativa original incompleta; veja o log'}
Write-Output 'Inicializadores IOP validados. O jogo completo ainda nao esta jogavel.'
