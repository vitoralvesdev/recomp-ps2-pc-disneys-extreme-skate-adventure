$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$toolPath = Join-Path $projectRoot 'tools\PS2Recomp'
$patchPath = Join-Path $projectRoot 'vendor\PS2Recomp-port.patch'
$pinnedCommit = 'e5636981a1c9de368f6b71e819c3dc188dfedbf4'
$repository = 'https://github.com/ran-j/PS2Recomp.git'

if (-not (Test-Path -LiteralPath $patchPath -PathType Leaf)) {
    throw "Patch do PS2Recomp ausente: $patchPath"
}
$git = Get-Command git.exe -ErrorAction SilentlyContinue
if (-not $git) { throw 'Git for Windows é necessário para obter o PS2Recomp.' }

if (Test-Path -LiteralPath $toolPath) {
    if (-not (Test-Path -LiteralPath (Join-Path $toolPath '.git') -PathType Container)) {
        throw "Já existe uma pasta sem checkout Git em '$toolPath'; ela foi preservada. Mova-a manualmente se quiser refazer o bootstrap."
    }
    $head = & $git.Source -C $toolPath rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $head.Trim() -ne $pinnedCommit) {
        throw "O checkout PS2Recomp existente precisa estar no commit $pinnedCommit. A pasta foi preservada."
    }
    & $git.Source -C $toolPath apply --reverse --check $patchPath 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Output "PS2Recomp $pinnedCommit já está preparado."
        return
    }
    & $git.Source -C $toolPath apply --check $patchPath 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'O checkout PS2Recomp existente não corresponde ao patch fixado; a pasta foi preservada.' }
    & $git.Source -C $toolPath apply $patchPath
    if ($LASTEXITCODE -ne 0) { throw 'Não foi possível aplicar o patch do PS2Recomp.' }
    Write-Output 'Patch do PS2Recomp aplicado ao checkout existente.'
    return
}

$toolsRoot = Split-Path $toolPath -Parent
New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null
& $git.Source -c core.longpaths=true clone --no-checkout $repository $toolPath
if ($LASTEXITCODE -ne 0) { throw 'Não foi possível clonar o PS2Recomp. Confira a conexão e tente novamente.' }
& $git.Source -C $toolPath config core.longpaths true
if ($LASTEXITCODE -ne 0) { throw 'Não foi possível habilitar caminhos longos no checkout PS2Recomp.' }
& $git.Source -C $toolPath checkout --detach $pinnedCommit
if ($LASTEXITCODE -ne 0) { throw "O commit fixado do PS2Recomp não foi encontrado: $pinnedCommit" }
& $git.Source -C $toolPath apply $patchPath
if ($LASTEXITCODE -ne 0) { throw 'Não foi possível aplicar o patch do port ao PS2Recomp.' }
Write-Output "PS2Recomp $pinnedCommit obtido e patch do port aplicado."
