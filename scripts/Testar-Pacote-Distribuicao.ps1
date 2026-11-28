param(
    [Parameter(Mandatory)][string]$PackageRoot,
    [string]$ProjectRoot = '',
    [switch]$PularVersao
)

$ErrorActionPreference = 'Stop'
if (-not $ProjectRoot) { $ProjectRoot = Split-Path $PSScriptRoot -Parent }
Import-Module (Join-Path $ProjectRoot 'scripts/Package.Common.psm1')
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$version = Get-DisneyPackageVersion $ProjectRoot
$launcherName = "Disney's Extreme Skate Adventure.exe"
$required = @(
    $launcherName, 'README.md', 'BUILD-INFO.txt', 'package-files.sha256',
    'scripts/Build.Common.psm1', 'scripts/Preparar-Dados-Nativos.ps1',
    'scripts/Instalar-Audios-Originais.py', 'scripts/Recuperar-Trilha-ISO.ps1',
    'scripts/music-recovery/install_music.py',
    'tools/python/python.exe', 'tools/python/python312.zip', 'tools/python/LICENSE.txt',
    'tools/col8-converter/Col8Converter.exe', 'tools/col8-converter/UPSTREAM-LICENSE.md',
    'tools/music-recovery/DisneyMusicRecover.exe',
    'tools/neversoft-multitool-v1.3.4/cli/NeversoftMultitool.exe',
    'tools/neversoft-multitool-v1.3.4/LICENSE.md',
    'third-party-notices/PROVENANCE.md', 'third-party-notices/MSVC-REDIST.txt',
    'third-party-notices/DOTNET-LICENSE.txt', 'third-party-notices/DOTNET-THIRD-PARTY.txt'
) + @((Get-DisneyRuntimeFileNames) | ForEach-Object { "runtime/$_" })
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $PackageRoot $relative) -PathType Leaf)) {
        throw "Arquivo obrigatorio ausente: $relative"
    }
}
$musicRecoveryScript = Get-Content -LiteralPath (Join-Path $PackageRoot 'scripts/Recuperar-Trilha-ISO.ps1') -Raw
$moduleImport = $musicRecoveryScript.IndexOf("Import-Module (Join-Path `$PSScriptRoot 'Build.Common.psm1')")
$firstProgress = $musicRecoveryScript.IndexOf('Write-DisneyProgress')
if ($moduleImport -lt 0 -or $firstProgress -lt 0 -or $moduleImport -gt $firstProgress) {
    throw 'Recuperar-Trilha-ISO.ps1 usa Write-DisneyProgress antes de importar Build.Common.psm1.'
}

$forbiddenDirectories = @('original', 'assets', 'game', 'mc0', 'mc1', 'analysis', 'logs', 'build', 'dist', 'saves')
$forbiddenExtensions = @('.iso', '.chd', '.cso', '.cue', '.wad', '.hed', '.elf', '.irx', '.pss', '.vag',
    '.bin', '.pdb', '.log', '.7z', '.rar', '.dmp', '.dump', '.pfx', '.pem', '.key')
$allowedRoot = @($launcherName, 'README.md', 'BUILD-INFO.txt', 'package-files.sha256', 'launcher.portable')
$allowedScripts = @('scripts/Build.Common.psm1', 'scripts/Preparar-Dados-Nativos.ps1',
    'scripts/Instalar-Audios-Originais.py', 'scripts/Recuperar-Trilha-ISO.ps1',
    'scripts/music-recovery/install_music.py')
$allowedDocs = @('docs/launcher.md', 'docs/controles.md', 'docs/DISTRIBUTION.md', 'docs/teste-nativo.md')
$allowedTools = @('tools/col8-converter/Col8Converter.exe', 'tools/col8-converter/UPSTREAM-LICENSE.md',
    'tools/music-recovery/DisneyMusicRecover.exe',
    'tools/neversoft-multitool-v1.3.4/cli/NeversoftMultitool.exe',
    'tools/neversoft-multitool-v1.3.4/LICENSE.md')
$files = Get-ChildItem -LiteralPath $PackageRoot -Recurse -File
foreach ($file in $files) {
    $relative = $file.FullName.Substring($PackageRoot.Length).TrimStart('\').Replace('\', '/')
    $parts = $relative.Split('/')
    if ($parts | Where-Object { $forbiddenDirectories -contains $_ }) {
        throw "Diretorio proibido no pacote: $relative"
    }
    if ($forbiddenExtensions -contains $file.Extension.ToLowerInvariant()) {
        throw "Extensao proibida no pacote: $relative"
    }
    if ($relative -match '(?i)(cover\.jpg|capa-jogo\.png|launcher-probe-result|install-state|package-manifest)') {
        throw "Arquivo privado ou de estado no pacote: $relative"
    }
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Link no pacote: $relative" }
    if ($relative -eq 'launcher.portable' -and $file.Length -ne 0) { throw 'launcher.portable deve ser um marcador vazio.' }
    $allowed = $allowedRoot -contains $relative -or $allowedScripts -contains $relative -or
        $allowedDocs -contains $relative -or $allowedTools -contains $relative -or
        $relative.StartsWith('tools/python/') -or $relative.StartsWith('third-party-notices/') -or
        ((Get-DisneyRuntimeFileNames) | ForEach-Object { "runtime/$_" }) -contains $relative
    if (-not $allowed) { throw "Arquivo fora da allowlist: $relative" }
    if ($file.Extension -in @('.exe', '.dll', '.pyd') -and -not (Test-DisneyPeX64 $file.FullName)) {
        throw "Binario nao e PE x64: $relative"
    }
}

$manifest = Join-Path $PackageRoot 'package-files.sha256'
$manifestEntries = @{}
foreach ($line in [IO.File]::ReadAllLines($manifest)) {
    $match = [regex]::Match($line, '^([0-9a-f]{64})  (.+)$')
    if (-not $match.Success) { throw "Linha invalida no manifesto: $line" }
    $relative = $match.Groups[2].Value.Replace('/', '\')
    $target = [IO.Path]::GetFullPath((Join-Path $PackageRoot $relative))
    $root = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\') + '\'
    if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Caminho inseguro no manifesto: $relative"
    }
    if ($manifestEntries.ContainsKey($relative)) { throw "Entrada duplicada no manifesto: $relative" }
    $manifestEntries[$relative] = $match.Groups[1].Value
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Arquivo do manifesto ausente: $relative" }
    if ((Get-DisneyFileHash $target) -ne $manifestEntries[$relative]) { throw "Hash divergente: $relative" }
}
$payloadFiles = @($files | Where-Object { $_.FullName -ne $manifest -and $_.Name -ne 'launcher.portable' })
if ($payloadFiles.Count -ne $manifestEntries.Count) { throw 'Manifesto nao cobre exatamente o payload.' }

$buildInfo = @{}
foreach ($line in [IO.File]::ReadAllLines((Join-Path $PackageRoot 'BUILD-INFO.txt'))) {
    $separator = $line.IndexOf('=')
    if ($separator -le 0) { throw "Linha invalida em BUILD-INFO.txt: $line" }
    $key = $line.Substring(0, $separator)
    if ($buildInfo.ContainsKey($key)) { throw "Chave duplicada em BUILD-INFO.txt: $key" }
    $buildInfo[$key] = $line.Substring($separator + 1)
}
if ($buildInfo['source_commit'] -notmatch '^[0-9a-f]{40}$') { throw 'source_commit invalido em BUILD-INFO.txt.' }
if ($buildInfo['source_dirty'] -notin @('true', 'false')) { throw 'source_dirty invalido em BUILD-INFO.txt.' }
if ($buildInfo['source_state_sha256'] -notmatch '^[0-9a-f]{64}$') { throw 'source_state_sha256 invalido em BUILD-INFO.txt.' }
if ($buildInfo['package_format'] -ne '1') { throw 'package_format invalido em BUILD-INFO.txt.' }
if ($buildInfo['runtime_source'] -notin @('local_directory', 'runtime_zip')) { throw 'runtime_source invalido em BUILD-INFO.txt.' }
if ($buildInfo['launcher_version'] -ne $version) { throw 'Versao divergente em BUILD-INFO.txt.' }
if ($buildInfo['dotnet_sdk'] -notmatch '^9\.[0-9]+\.[0-9]+$') { throw 'dotnet_sdk invalido em BUILD-INFO.txt.' }
if ($buildInfo['dotnet_runtime'] -notmatch '^9\.[0-9]+\.[0-9]+$') { throw 'dotnet_runtime invalido em BUILD-INFO.txt.' }
foreach ($key in @('python_sha256', 'neversoft_sha256')) {
    if ($buildInfo[$key] -notmatch '^[0-9a-f]{64}$') { throw "$key invalido em BUILD-INFO.txt." }
}
$componentHashes = @{
    launcher_sha256 = $launcherName
    runtime_sha256 = 'runtime/DisneySkateRuntime.exe'
    col8_sha256 = 'tools/col8-converter/Col8Converter.exe'
    music_recovery_sha256 = 'tools/music-recovery/DisneyMusicRecover.exe'
}
foreach ($key in ($componentHashes.Keys | Sort-Object)) {
    $relative = $componentHashes[$key]
    if ($buildInfo[$key] -ne (Get-DisneyFileHash (Join-Path $PackageRoot $relative))) {
        throw "Hash de $relative divergente em BUILD-INFO.txt."
    }
}

if (-not $PularVersao) {
    $launcherResult = Invoke-DisneyExecutable (Join-Path $PackageRoot $launcherName) @('--versao')
    if ($launcherResult.ExitCode -ne 0 -or $launcherResult.StandardOutput -notmatch [regex]::Escape($version)) {
        throw "Versao executada divergente: $($launcherResult.StandardOutput)$($launcherResult.StandardError)"
    }
}
$pythonResult = Invoke-DisneyExecutable (Join-Path $PackageRoot 'tools/python/python.exe') @('-c', 'import sys; print(sys.version_info[:3])')
if ($pythonResult.ExitCode -ne 0 -or $pythonResult.StandardOutput -notmatch '\(3, 12, 10\)') {
    throw "Python incorporado divergente: $($pythonResult.StandardOutput)$($pythonResult.StandardError)"
}
Write-Output "PASS: $version, $($files.Count) arquivos, allowlist, hashes, ferramentas e PE x64 validados."
