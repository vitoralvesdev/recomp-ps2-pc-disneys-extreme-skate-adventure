param([ValidateSet('initial','ghidra')][string]$Config = 'ghidra')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$recompiler = Join-Path $projectRoot 'build/ps2recomp/ps2xRecomp/Release/ps2_recomp.exe'
if (-not (Test-Path -LiteralPath $recompiler -PathType Leaf)) {
 & (Join-Path $PSScriptRoot 'Compilar-Ferramentas.ps1')
 if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Falha ao compilar PS2Recomp.' }
}
if (-not (Test-Path -LiteralPath $recompiler -PathType Leaf)) { throw "Executável PS2Recomp não foi produzido: $recompiler" }
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'analysis') | Out-Null
Push-Location "$projectRoot\recomp"
try {
 & $recompiler "$Config.toml" *> "$projectRoot\analysis\$Config-recomp.log"
 if ($LASTEXITCODE -ne 0) { throw "Falha; consulte analysis/$Config-recomp.log" }
} finally { Pop-Location }
