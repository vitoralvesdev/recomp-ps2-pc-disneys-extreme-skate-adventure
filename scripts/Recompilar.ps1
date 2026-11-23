param(
    [ValidateSet('initial','ghidra')][string]$Config = 'ghidra',
    [string]$ElfPath = '',
    [string]$OutputDir = '',
    [string]$FunctionsCsv = '',
    [string]$BuildDir = '',
    [string]$AnalysisDir = '',
    [switch]$PrepareGuestEntries
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if(-not $ElfPath){$ElfPath=Join-Path $projectRoot 'original/extracted/SLUS_206.07'}
if(-not $OutputDir){$OutputDir=Join-Path $projectRoot $(if($Config -eq 'initial'){'src/generated-initial'}else{'src/generated-ghidra'})}
if(-not $FunctionsCsv){$FunctionsCsv=Join-Path $projectRoot 'analysis/functions.csv'}
if(-not $BuildDir){$BuildDir=Join-Path $projectRoot 'build'}
if(-not $AnalysisDir){$AnalysisDir=Join-Path $projectRoot 'analysis'}
$ElfPath=[IO.Path]::GetFullPath($ElfPath)
$OutputDir=[IO.Path]::GetFullPath($OutputDir)
$FunctionsCsv=[IO.Path]::GetFullPath($FunctionsCsv)
$BuildDir=[IO.Path]::GetFullPath($BuildDir)
$AnalysisDir=[IO.Path]::GetFullPath($AnalysisDir)
if(-not (Test-Path -LiteralPath $ElfPath -PathType Leaf)){throw "ELF de entrada ausente: $ElfPath"}
if($PrepareGuestEntries -and $Config -ne 'ghidra'){throw 'PrepareGuestEntries requer a configuracao ghidra.'}
New-Item -ItemType Directory -Force -Path $OutputDir, (Split-Path $FunctionsCsv -Parent), $AnalysisDir | Out-Null
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$recompiler = Join-Path $BuildDir 'ps2recomp/ps2xRecomp/Release/ps2_recomp.exe'
Write-DisneyProgress 3 'Preparando o PS2Recomp fixado...'
if (-not (Test-Path -LiteralPath $recompiler -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'Compilar-Ferramentas.ps1') -BuildDir $BuildDir
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Falha ao compilar PS2Recomp.' }
}
if (-not (Test-Path -LiteralPath $recompiler -PathType Leaf)) { throw "Executavel PS2Recomp nao foi produzido: $recompiler" }
$ghidraOutput=if($Config -eq 'ghidra'){$FunctionsCsv}else{''}
$configPath=Join-Path $AnalysisDir "launcher-$Config-recomp.toml"
$configText="input = $(ConvertTo-Json -InputObject ($ElfPath.Replace('\','/')) -Compress)`n" +
            "output = $(ConvertTo-Json -InputObject ($OutputDir.Replace('\','/')) -Compress)`n" +
            "ghidra_output = $(ConvertTo-Json -InputObject ($ghidraOutput.Replace('\','/')) -Compress)`n"
[IO.File]::WriteAllText($configPath,$configText,(New-Object Text.UTF8Encoding($false)))
$logPath=Join-Path $AnalysisDir "$Config-recomp.log"
Write-DisneyProgress 10 'Recompilando as funcoes EE a partir do ELF extraido...'
$previousErrorAction=$ErrorActionPreference
try {
    $ErrorActionPreference='Continue'
    & $recompiler $configPath 2>&1 | Tee-Object -FilePath $logPath
    $recompilerExit=$LASTEXITCODE
} finally {$ErrorActionPreference=$previousErrorAction}
if ($recompilerExit -ne 0) { throw "Falha na recompilacao; consulte $logPath" }
Write-DisneyProgress 75 'Funcoes EE recompiladas.'
if($PrepareGuestEntries){
    $python=Get-Command python.exe -ErrorAction SilentlyContinue
    if(-not $python){$python=Get-Command python -ErrorAction SilentlyContinue}
    if(-not $python){throw 'Python 3 e necessario para preparar as entradas e os lotes do convidado.'}
    Write-DisneyProgress 80 'Preparando entradas recuperadas e lotes de compilacao...'
    & $python.Source (Join-Path $PSScriptRoot 'prepare_guest_entries.py') --elf $ElfPath --functions-csv $FunctionsCsv --generated-dir $OutputDir --extra-entry-points (Join-Path $projectRoot 'recomp/extra-entry-points.json') --disjoint-functions (Join-Path $projectRoot 'analysis/disjoint-initial-functions.json') --guest-output (Join-Path $projectRoot 'src/host/guest_entries.cpp') --report (Join-Path $AnalysisDir 'recovered-switch-targets.json')
    if($LASTEXITCODE -ne 0){throw 'Falha ao preparar as entradas recuperadas.'}
    & $python.Source (Join-Path $PSScriptRoot 'prepare_batches.py') --generated-dir $OutputDir --bridges (Join-Path $projectRoot 'recomp/host-bridges.json') --output (Join-Path $BuildDir 'guest-batches')
    if($LASTEXITCODE -ne 0){throw 'Falha ao preparar os lotes de compilacao.'}
}
Write-DisneyProgress 100 'Recompilacao e preparacao concluidas.'
