$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$cmake=Join-Path $projectRoot 'tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe'
$source=Join-Path $PSScriptRoot 'music-recovery'
$build=Join-Path $projectRoot 'build/music-recovery'
$output=Join-Path $build 'output'
New-Item -ItemType Directory -Path $output -Force | Out-Null
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64
if($LASTEXITCODE -ne 0){throw 'Falha ao configurar conversor offline'}
& $cmake --build $build --config Release --parallel 2
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar conversor offline'}
& (Join-Path $build 'Release/DisneyMusicRecover.exe') $projectRoot (Join-Path $output 'pcm') | Set-Content -LiteralPath (Join-Path $output 'decoded.tsv') -Encoding utf8
if($LASTEXITCODE -ne 0){throw 'Falha ao converter MUSIC.WAD'}
& python (Join-Path $source 'install_music.py') $projectRoot $output
if($LASTEXITCODE -ne 0){throw 'Falha ao validar/instalar musicas'}
