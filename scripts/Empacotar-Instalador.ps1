param(
    [switch]$SemCompilar
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent

if (-not $SemCompilar) {
    & (Join-Path $PSScriptRoot 'Compilar-Launcher.ps1') -SemTestes
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Falha ao compilar o launcher.' }
}

$launcher = Join-Path $projectRoot "Disney's Extreme Skate Adventure.exe"
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "Launcher nao encontrado: $launcher"
}
$runtime = Join-Path $projectRoot 'runtime\DisneySkateRuntime.exe'
if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) {
    throw "Runtime nao encontrado: $runtime. Compile o runtime ou use um pacote pronto."
}

$isccCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
)
$iscc = $isccCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $iscc) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) { $iscc = $command.Source }
}
if (-not $iscc) {
    throw 'Inno Setup 6 (ISCC.exe) nao encontrado. Instale em https://jrsoftware.org/isinfo.php e repita.'
}

$script = Join-Path $projectRoot 'installer\DisneySkateLauncher.iss'
$dist = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null

Write-Output 'Gerando o instalador com Inno Setup...'
& $iscc $script
if ($LASTEXITCODE -ne 0) { throw 'Falha ao gerar o instalador.' }

$setup = Join-Path $dist 'DisneySkateSetup-0.4.0.exe'
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "O Inno Setup terminou sem produzir $setup"
}
Write-Output ''
Write-Output "Instalador gerado: $setup"
Write-Output 'O instalador contem o launcher, o runtime e os scripts/fontes de preparacao; a ISO, os pacotes de audio, os dados do jogo, os assets gerados e ferramentas de terceiros nao sao incluidos.'
