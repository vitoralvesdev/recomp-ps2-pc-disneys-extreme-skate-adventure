$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$env:PATH=(Join-Path $projectRoot 'runtime')+';'+$env:PATH
$cmake=Join-Path $projectRoot 'tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe'
& $cmake --build (Join-Path $projectRoot 'build/port') --config Release --target DisneyNativeGifStreamTest DisneyOriginalGraphicsReplay --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar teste grafico'}
& (Join-Path $projectRoot 'dist/native-tools/Release/DisneyNativeGifStreamTest.exe') (Join-Path $projectRoot 'analysis/native-gif-before-stream-fix.bin')
if($LASTEXITCODE -ne 0){throw 'Falha na regressao de transferencias graficas'}

& (Join-Path $projectRoot 'dist/native-tools/Release/DisneyOriginalGraphicsReplay.exe') --check-disney-job (Join-Path $projectRoot 'analysis/native-vu-regression-job222.bin')
if($LASTEXITCODE -ne 0){throw 'Falha na regressao do VU1 original Disney'}

& (Join-Path $projectRoot 'dist/native-tools/Release/DisneyOriginalGraphicsReplay.exe') --check-image2-job (Join-Path $projectRoot 'analysis/native-vu-regression-job398.bin')
if($LASTEXITCODE -ne 0){throw 'Falha na regressao do marcador IMAGE2 original'}

& (Join-Path $projectRoot 'dist/native-tools/Release/DisneyOriginalGraphicsReplay.exe') --pad-test
if($LASTEXITCODE -ne 0){throw 'Falha na ordem dos botoes Disney'}
