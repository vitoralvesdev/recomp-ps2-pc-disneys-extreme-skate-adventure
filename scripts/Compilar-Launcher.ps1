param(
    [string]$Iso,
    [switch]$SemTestes
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$cmake = Get-DisneyCMake
$buildDir = Join-Path $projectRoot 'build\launcher'

if ($Iso) {
    if (-not (Test-Path -LiteralPath $Iso -PathType Leaf)) {
        throw "ISO nao encontrada: $Iso"
    }
    $Iso = (Resolve-Path -LiteralPath $Iso).Path
}

Write-Output 'Configurando o launcher (sem dados do jogo)...'
& $cmake -S $projectRoot -B $buildDir -G 'Visual Studio 17 2022' -A x64 `
    '-DDISNEY_BUILD_LAUNCHER_ONLY=ON'
if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar o launcher.' }

Write-Output 'Compilando launcher e testes...'
& $cmake --build $buildDir --config Release --target DisneySkateLauncher DisneyLauncherTests --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o launcher.' }

$launcher = Join-Path $projectRoot "Disney's Extreme Skate Adventure.exe"
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "O build terminou sem produzir $launcher"
}

if (-not $SemTestes) {
    $testExe = Join-Path $buildDir 'launcher-tests\Release\DisneyLauncherTests.exe'
    if (-not (Test-Path -LiteralPath $testExe -PathType Leaf)) {
        throw "Executavel de testes nao encontrado: $testExe"
    }
    if ($Iso) {
        $env:DISNEY_TEST_ISO = $Iso
        $extracted = Join-Path $projectRoot 'original\extracted'
        if (Test-Path -LiteralPath $extracted -PathType Container) {
            $env:DISNEY_TEST_EXTRACTED = $extracted
        }
    }
    Push-Location $buildDir
    try {
        & $testExe
        if ($LASTEXITCODE -ne 0) { throw 'Os testes do launcher falharam.' }
    } finally {
        Pop-Location
        Remove-Item Env:\DISNEY_TEST_ISO -ErrorAction SilentlyContinue
        Remove-Item Env:\DISNEY_TEST_EXTRACTED -ErrorAction SilentlyContinue
    }
}

Write-Output ''
Write-Output "Launcher compilado: $launcher"
Write-Output 'Abra o executavel para instalar/abrir o jogo.'
