$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$cmake = "$projectRoot\tools\cmake\cmake-4.4.4-windows-x86_64\bin\cmake.exe"
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'core.longpaths'
$env:GIT_CONFIG_VALUE_0 = 'true'
& $cmake -S "$projectRoot\tools\PS2Recomp" -B "$projectRoot\build\ps2recomp" -G 'Visual Studio 17 2022' -A x64 '-DFETCHCONTENT_BASE_DIR=C:/Users/vitor/OneDrive/Documentos/ChatGPT/REA/deps-ps2' -DPS2X_BUILD_STUDIO=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_BUILD_RUNTIME=OFF
if ($LASTEXITCODE -ne 0) { throw 'Falha configurando' }
& $cmake --build "$projectRoot\build\ps2recomp" --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Falha compilando' }
