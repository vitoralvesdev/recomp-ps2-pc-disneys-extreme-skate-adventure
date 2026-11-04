$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'Compilar-Candidato-Original.ps1')
exit $LASTEXITCODE
