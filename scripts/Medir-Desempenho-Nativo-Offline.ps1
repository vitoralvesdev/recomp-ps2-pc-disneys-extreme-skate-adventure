param([int]$Repeticoes=2,[switch]$CompararGPU,[switch]$SomenteVU)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$test=Join-Path $projectRoot 'dist/native-tools/Release/DisneyOriginalGraphicsReplay.exe'
$oldPath=$env:PATH;$oldScale=$env:PS2X_GS_HW_SCALE;$oldShared=$env:PS2X_GS_DIRECT_PRESENT
try {
 $env:PATH=(Join-Path $projectRoot 'runtime')+';'+$env:PATH
 $env:PS2X_GS_HW_SCALE='1';$env:PS2X_GS_DIRECT_PRESENT='0'
 if($CompararGPU -and $SomenteVU){throw 'Escolha CompararGPU ou SomenteVU'}
 $mode=if($SomenteVU){'--benchmark-vu'}elseif($CompararGPU){'--qualify-gpu'}else{'--profile-captured'}
 & $test $mode (Join-Path $projectRoot 'analysis') ([Math]::Max(1,$Repeticoes))
 if($LASTEXITCODE -ne 0){throw 'Falha na reproducao offline'}
} finally {$env:PATH=$oldPath;$env:PS2X_GS_HW_SCALE=$oldScale;$env:PS2X_GS_DIRECT_PRESENT=$oldShared}
