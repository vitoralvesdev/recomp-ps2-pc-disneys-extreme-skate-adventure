param([ValidateSet('initial','ghidra')][string]$Config = 'ghidra')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location "$projectRoot\recomp"
try {
 & "$projectRoot\build\ps2recomp\ps2xRecomp\Release\ps2_recomp.exe" "$Config.toml" *> "$projectRoot\analysis\$Config-recomp.log"
 if ($LASTEXITCODE -ne 0) { throw "Falha; consulte analysis/$Config-recomp.log" }
} finally { Pop-Location }
