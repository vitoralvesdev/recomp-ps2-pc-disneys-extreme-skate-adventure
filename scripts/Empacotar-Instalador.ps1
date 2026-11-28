param(
    [switch]$SemCompilar,
    [switch]$TestInstall,
    [switch]$AllowDirtyRuntimeZip,
    [string]$RuntimeZip = '',
    [string]$RuntimeSha256 = '',
    [string]$RuntimeDirectory = '',
    [string]$NeversoftZip = '',
    [string]$NeversoftSha256 = '062c89e00382e7b7c4b73015f228c88e7be34e50a4c48654d39e3142bd633d35',
    [string]$PythonZip = '',
    [string]$PythonSha256 = '4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3',
    [string]$RedistDirectory = '',
    [string]$OutputDirectory = '',
    [string]$PfxPath = '',
    [string]$TimestampUrl = 'http://timestamp.digicert.com'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'Package.Common.psm1')
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$source = Get-DisneySourceState $projectRoot
$version = $source.launcher_version
$launcherName = "Disney's Extreme Skate Adventure.exe"
$launcher = Join-Path $projectRoot $launcherName
$toolsBuild = Join-Path $projectRoot 'build/package-tools'
$dist = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $projectRoot 'dist' }
$payload = Join-Path $projectRoot "build/package/DisneySkate-$version"
$work = Join-Path $projectRoot ('build/package-work/' + [Guid]::NewGuid().ToString('N'))
$col8Output = Join-Path $toolsBuild 'col8'
$musicOutput = Join-Path $toolsBuild 'music'
$runtimeRoot = ''
$runtimeInfo = $null
$noticesSource = Join-Path $projectRoot 'docs/third-party-runtime'

if (-not $PythonZip) { $PythonZip = 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip' }
if (-not $NeversoftZip) {
    $NeversoftZip = 'https://github.com/slfx77/neversoft-multitool/releases/download/v1.3.4/NeversoftMultitool-Windows-CLI-x64.zip'
}

function Invoke-DisneyBuild {
    param([Parameter(Mandatory)][string]$FilePath, [Parameter(Mandatory)][string[]]$Arguments, [string]$Message)
    if ($Message) { Write-Output $Message }
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Comando falhou ($LASTEXITCODE): $FilePath $($Arguments -join ' ')" }
}

function Read-DisneyBuildInfo {
    param([Parameter(Mandatory)][string]$Path)
    $values = @{}
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) { throw "Linha invalida em BUILD-INFO.txt: $line" }
        $key = $line.Substring(0, $separator)
        if ($values.ContainsKey($key)) { throw "Chave duplicada em BUILD-INFO.txt: $key" }
        $values[$key] = $line.Substring($separator + 1)
    }
    return $values
}

function Set-DisneyAuthenticode {
    param([Parameter(Mandatory)][string]$Path)
    if (-not $PfxPath) { return }
    $certificate = (Resolve-Path -LiteralPath $PfxPath).Path
    $password = [Environment]::GetEnvironmentVariable('DISNEY_SIGNING_PASSWORD')
    if (-not $password) { throw 'Defina DISNEY_SIGNING_PASSWORD para assinar com -PfxPath.' }
    $signTool = Get-Command signtool.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    if (-not $signTool) {
        $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
        if (Test-Path -LiteralPath $kitsRoot -PathType Container) {
            $signTool = Get-ChildItem -LiteralPath $kitsRoot -Directory -Filter '10.*' |
                Sort-Object { [version]$_.Name } -Descending |
                ForEach-Object { Join-Path $_.FullName 'x64/signtool.exe' } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        }
    }
    if (-not $signTool) { throw 'signtool.exe nao encontrado no PATH nem no Windows SDK.' }
    Invoke-DisneyBuild $signTool @('sign', '/fd', 'SHA256', '/f', $certificate, '/p', $password,
        '/tr', $TimestampUrl, '/td', 'SHA256', $Path) "Assinando $([IO.Path]::GetFileName($Path))..."
    Invoke-DisneyBuild $signTool @('verify', '/pa', '/tw', $Path) 'Verificando a assinatura Authenticode...'
}

New-Item -ItemType Directory -Force -Path $dist, (Split-Path -Parent $work), (Split-Path -Parent $payload) | Out-Null
try {
    $dotnetVersion = (& dotnet --version).Trim()
    $runtimeCandidates = @(& dotnet --list-runtimes | Where-Object {
        $_ -match '^Microsoft\.NETCore\.App (9\.[0-9]+\.[0-9]+) '
    } | ForEach-Object { [version]$Matches[1] })
    $runtimeMaximum = ($runtimeCandidates | Measure-Object -Maximum).Maximum
    if (-not $runtimeMaximum) { throw 'Runtime .NET 9 nao encontrado para a publicacao autocontida.' }
    $dotnetRuntimeVersion = $runtimeMaximum.ToString()

    if (-not $SemCompilar) {
        Write-Output "[1/7] Compilando launcher e testes..."
        & (Join-Path $PSScriptRoot 'Compilar-Launcher.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar ou testar o launcher.' }

        Write-Output "[2/7] Publicando COL8 autocontido..."
        if (Test-Path -LiteralPath $col8Output) { Remove-Item -LiteralPath $col8Output -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $col8Output | Out-Null
        Invoke-DisneyBuild 'dotnet' @('publish', (Join-Path $projectRoot 'tools-src/col8-converter/Col8Converter.csproj'),
            '-c', 'Release', '-r', 'win-x64', '--self-contained', 'true', "-p:RuntimeFrameworkVersion=$dotnetRuntimeVersion",
            '-p:PublishSingleFile=true', '-p:IncludeNativeLibrariesForSelfExtract=true',
            '-p:UseSharedCompilation=false', '--source', 'https://api.nuget.org/v3/index.json',
            '-o', $col8Output) 'Publicando Col8Converter.exe...'

        Write-Output "[3/7] Compilando recuperador de musica..."
        $cmake = Get-DisneyCMake
        if (Test-Path -LiteralPath $musicOutput) { Remove-Item -LiteralPath $musicOutput -Recurse -Force }
        Invoke-DisneyBuild $cmake @('-S', (Join-Path $projectRoot 'scripts/music-recovery'), '-B', $musicOutput,
            '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW',
            '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DCMAKE_BUILD_TYPE=Release') 'Configurando DisneyMusicRecover...'
        Invoke-DisneyBuild $cmake @('--build', $musicOutput, '--config', 'Release', '--target', 'DisneyMusicRecover', '--parallel', '2')
    } else {
        foreach ($required in @($launcher, (Join-Path $col8Output 'Col8Converter.exe'), (Join-Path $musicOutput 'Release/DisneyMusicRecover.exe'))) {
            if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Build reutilizavel ausente: $required" }
        }
    }

    if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw "Launcher ausente: $launcher" }
    if (-not (Test-Path -LiteralPath (Join-Path $col8Output 'Col8Converter.exe') -PathType Leaf)) { throw 'Col8Converter.exe nao foi gerado.' }
    $musicExecutable = Join-Path $musicOutput 'Release/DisneyMusicRecover.exe'
    if (-not (Test-Path -LiteralPath $musicExecutable -PathType Leaf)) { throw 'DisneyMusicRecover.exe nao foi gerado.' }
    $dotnetRoot = Split-Path (Get-Command dotnet).Source -Parent
    $dotnetLicense = Join-Path $dotnetRoot 'LICENSE.txt'
    $dotnetThirdParty = Join-Path $dotnetRoot 'ThirdPartyNotices.txt'
    foreach ($required in @($dotnetLicense, $dotnetThirdParty)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Aviso .NET ausente: $required" }
    }

    Write-Output '[4/7] Preparando staging limpo e ferramentas incorporadas...'
    if (Test-Path -LiteralPath $payload) { Remove-Item -LiteralPath $payload -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $payload | Out-Null
    Copy-Item -LiteralPath $launcher -Destination (Join-Path $payload $launcherName) -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'installer/LEIA-ME.md') -Destination (Join-Path $payload 'README.md') -Force

    foreach ($relative in @(
        'scripts/Build.Common.psm1', 'scripts/Preparar-Dados-Nativos.ps1',
        'scripts/Instalar-Audios-Originais.py', 'scripts/Recuperar-Trilha-ISO.ps1',
        'scripts/music-recovery/install_music.py',
        'docs/launcher.md', 'docs/controles.md', 'docs/DISTRIBUTION.md', 'docs/teste-nativo.md')) {
        $destination = Join-Path $payload $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath (Join-Path $projectRoot $relative) -Destination $destination -Force
    }

    $pythonExtract = Join-Path $work 'python'
    Expand-DisneyZip -Source $PythonZip -Destination $pythonExtract -ExpectedSha256 $PythonSha256 -ProjectRoot $projectRoot | Out-Null
    $pythonDestination = Join-Path $payload 'tools/python'
    New-Item -ItemType Directory -Force -Path $pythonDestination | Out-Null
    Copy-Item -Path (Join-Path $pythonExtract '*') -Destination $pythonDestination -Recurse -Force

    $neversoftExtract = Join-Path $work 'neversoft'
    Expand-DisneyZip -Source $NeversoftZip -Destination $neversoftExtract -ExpectedSha256 $NeversoftSha256 -ProjectRoot $projectRoot | Out-Null
    $neversoftExecutable = Find-DisneyNeversoft $neversoftExtract
    $neversoftDestination = Join-Path $payload 'tools/neversoft-multitool-v1.3.4/cli'
    New-Item -ItemType Directory -Force -Path $neversoftDestination, (Join-Path $payload 'tools/col8-converter'), (Join-Path $payload 'tools/music-recovery') | Out-Null
    Copy-Item -LiteralPath $neversoftExecutable -Destination (Join-Path $neversoftDestination 'NeversoftMultitool.exe') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/third-party-runtime/NeversoftMultitool-LICENSE') -Destination (Join-Path $payload 'tools/neversoft-multitool-v1.3.4/LICENSE.md') -Force
    Copy-Item -LiteralPath (Join-Path $col8Output 'Col8Converter.exe') -Destination (Join-Path $payload 'tools/col8-converter/Col8Converter.exe') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tools-src/col8-converter/UPSTREAM-LICENSE.md') -Destination (Join-Path $payload 'tools/col8-converter/UPSTREAM-LICENSE.md') -Force
    Copy-Item -LiteralPath $musicExecutable -Destination (Join-Path $payload 'tools/music-recovery/DisneyMusicRecover.exe') -Force

    Write-Output '[5/7] Validando runtime, procedência e assinatura do launcher...'
    if ($RuntimeZip) {
        if (-not $RuntimeSha256) { throw 'Informe -RuntimeSha256 ao usar -RuntimeZip.' }
        $runtimeExtract = Join-Path $work 'runtime'
        Expand-DisneyZip -Source $RuntimeZip -Destination $runtimeExtract -ExpectedSha256 $RuntimeSha256 -ProjectRoot $projectRoot | Out-Null
        $infoFiles = @(Get-ChildItem -LiteralPath $runtimeExtract -Recurse -File -Filter 'BUILD-INFO.txt')
        if ($infoFiles.Count -ne 1) { throw 'O ZIP de runtime deve conter exatamente um BUILD-INFO.txt.' }
        $runtimeInfo = Read-DisneyBuildInfo $infoFiles[0].FullName
        if ($runtimeInfo['source_commit'] -ne $source.source_commit) {
            throw "Runtime de outro commit: esperado $($source.source_commit), obtido $($runtimeInfo['source_commit'])."
        }
        if ($runtimeInfo['launcher_version'] -ne $version) { throw 'Versao do runtime divergente.' }
        if ($runtimeInfo['source_dirty'] -eq 'true' -and -not $AllowDirtyRuntimeZip) {
            throw 'O ZIP de runtime foi gerado com alteracoes locais. Use um artefato de commit confiavel ou -AllowDirtyRuntimeZip somente para validacao local.'
        }
        $runtimeRoot = $infoFiles[0].Directory.FullName
        $runtimeNotices = Join-Path $runtimeRoot 'notices'
        if (-not (Test-Path -LiteralPath $runtimeNotices -PathType Container)) { throw 'O ZIP de runtime nao contem a pasta notices auditada.' }
        $noticesSource = $runtimeNotices
    } else {
        $runtimeRoot = if ($RuntimeDirectory) { (Resolve-Path -LiteralPath $RuntimeDirectory).Path } else { Join-Path $projectRoot 'runtime' }
        $runtimeInfo = $source.Clone()
    }
    $runtimeDestination = Join-Path $payload 'runtime'
    New-Item -ItemType Directory -Force -Path $runtimeDestination | Out-Null
    foreach ($name in (Get-DisneyRuntimeFileNames)) {
        $candidate = Join-Path $runtimeRoot $name
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            if ($RuntimeZip) { throw "Componente ausente no ZIP de runtime: $name" }
            $candidate = Join-Path (Get-DisneyRedistDirectory $projectRoot $RedistDirectory) $name
        }
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "Componente de runtime ausente: $name" }
        Copy-Item -LiteralPath $candidate -Destination (Join-Path $runtimeDestination $name) -Force
    }
    foreach ($file in Get-ChildItem -LiteralPath $runtimeDestination -File) {
        if (-not (Test-DisneyPeX64 $file.FullName)) { throw "Runtime nao e PE x64: $($file.Name)" }
    }
    $runtimeInfo['runtime_sha256'] = Get-DisneyFileHash (Join-Path $runtimeDestination 'DisneySkateRuntime.exe')
    $noticesDestination = Join-Path $payload 'third-party-notices'
    New-Item -ItemType Directory -Force -Path $noticesDestination | Out-Null
    Copy-Item -Path (Join-Path $noticesSource '*') -Destination $noticesDestination -Recurse -Force
    Copy-Item -LiteralPath $dotnetLicense -Destination (Join-Path $noticesDestination 'DOTNET-LICENSE.txt') -Force
    Copy-Item -LiteralPath $dotnetThirdParty -Destination (Join-Path $noticesDestination 'DOTNET-THIRD-PARTY.txt') -Force

    Set-DisneyAuthenticode (Join-Path $payload $launcherName)
    $runtimeInfo['package_format'] = '1'
    $runtimeInfo['dotnet_sdk'] = $dotnetVersion
    $runtimeInfo['dotnet_runtime'] = $dotnetRuntimeVersion
    $runtimeInfo['launcher_sha256'] = Get-DisneyFileHash (Join-Path $payload $launcherName)
    $pythonArchive = if ($PythonZip -match '^https://') {
        Join-Path $projectRoot ('build/package-inputs/' + [IO.Path]::GetFileName(([Uri]$PythonZip).AbsolutePath))
    } else { $PythonZip }
    $neversoftArchive = if ($NeversoftZip -match '^https://') {
        Join-Path $projectRoot ('build/package-inputs/' + [IO.Path]::GetFileName(([Uri]$NeversoftZip).AbsolutePath))
    } else { $NeversoftZip }
    $runtimeInfo['python_sha256'] = Get-DisneyFileHash $pythonArchive
    $runtimeInfo['neversoft_sha256'] = Get-DisneyFileHash $neversoftArchive
    $runtimeInfo['col8_sha256'] = Get-DisneyFileHash (Join-Path $payload 'tools/col8-converter/Col8Converter.exe')
    $runtimeInfo['music_recovery_sha256'] = Get-DisneyFileHash (Join-Path $payload 'tools/music-recovery/DisneyMusicRecover.exe')
    $runtimeInfo['runtime_source'] = if ($RuntimeZip) { 'runtime_zip' } else { 'local_directory' }
    $buildInfoPath = Join-Path $payload 'BUILD-INFO.txt'
    $buildInfoLines = foreach ($key in ($runtimeInfo.Keys | Sort-Object)) { "$key=$($runtimeInfo[$key])" }
    [IO.File]::WriteAllLines($buildInfoPath, $buildInfoLines, (New-Object Text.UTF8Encoding($false)))

    Write-Output '[6/7] Validando allowlist, hashes e binarios...'
    Write-DisneyPayloadHashes $payload
    & (Join-Path $PSScriptRoot 'Testar-Pacote-Distribuicao.ps1') -PackageRoot $payload -ProjectRoot $projectRoot
    if ($LASTEXITCODE -ne 0) { throw 'A validacao do pacote falhou.' }

    $isccCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6/ISCC.exe')
    )
    $iscc = $isccCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $iscc) {
        $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($command) { $iscc = $command.Source }
    }
    if (-not $iscc) { throw 'Inno Setup 6 (ISCC.exe) nao encontrado. Instale em https://jrsoftware.org/isinfo.php e repita.' }

    $suffix = if ($TestInstall) { '-test' } else { '' }
    Write-Output '[7/7] Gerando instalador e ZIP portatil...'
    $setup = Join-Path $dist "DisneySkateSetup-$version$suffix.exe"
    $issArguments = @((Join-Path $projectRoot 'installer/DisneySkateLauncher.iss'),
        "/DMyAppVersion=$version", "/DVersionInfoVersion=$version.0", "/DPackageRoot=$payload",
        "/DDistDir=$dist", "/DInstallerSuffix=$suffix", "/DAppIcon=$(Join-Path $projectRoot 'src/launcher/app.ico')")
    if ($TestInstall) { $issArguments += '/DLAUNCHER_TEST_INSTALL' }
    Invoke-DisneyBuild $iscc $issArguments 'Compilando instalador Inno Setup...'
    if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) { throw "O Inno Setup terminou sem produzir $setup" }
    Set-DisneyAuthenticode $setup

    $portable = Join-Path $dist "DisneySkate-$version-portable.zip"
    Compress-DisneyPortable $payload $portable $version
    $portableCheck = Join-Path $work 'portable-check'
    Expand-DisneyZip -Source $portable -Destination $portableCheck
    $portableRoot = Join-Path $portableCheck "DisneySkate-$version"
    Invoke-DisneyBuild 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $PSScriptRoot 'Testar-Pacote-Distribuicao.ps1'), '-PackageRoot', $portableRoot) 'Validando ZIP portatil extraido...'
    $sums = @(
        "$(Get-DisneyFileHash $setup)  $([IO.Path]::GetFileName($setup))",
        "$(Get-DisneyFileHash $portable)  $([IO.Path]::GetFileName($portable))"
    )
    $sumsName = if ($TestInstall) { 'SHA256SUMS-test.txt' } else { 'SHA256SUMS.txt' }
    [IO.File]::WriteAllLines((Join-Path $dist $sumsName), $sums, (New-Object Text.UTF8Encoding($false)))
    Write-Output ''
    Write-Output "Instalador: $setup"
    Write-Output "Portatil: $portable"
    Write-Output "Checksums: $(Join-Path $dist $sumsName)"
    Write-Output 'A ISO, os dados extraidos, saves, logs, PDBs e capas soltas do jogo nao entram no pacote.'
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
