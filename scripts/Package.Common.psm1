$script:RuntimeFileNames = @(
    'DisneySkateRuntime.exe',
    'avcodec-61.dll', 'avdevice-61.dll', 'avfilter-10.dll', 'avformat-61.dll', 'avutil-59.dll',
    'brotlicommon.dll', 'brotlidec.dll', 'brotlienc.dll', 'freetype.dll', 'harfbuzz.dll',
    'Iex-3_3.dll', 'IlmThread-3_3.dll', 'Imath-3_1.dll', 'jxl.dll', 'jxl_cms.dll', 'jxl_threads.dll',
    'libsharpyuv.dll', 'libwebp.dll', 'libwebpdecoder.dll', 'libwebpdemux.dll', 'libwebpmux.dll',
    'OpenEXR-3_3.dll', 'OpenEXRCore-3_3.dll', 'OpenEXRUtil-3_3.dll', 'SDL2.dll',
    'swresample-5.dll', 'swscale-8.dll', 'zlib1.dll',
    'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'
)

function Get-DisneyRuntimeFileNames {
    return $script:RuntimeFileNames
}

function Get-DisneyPackageVersion {
    param([Parameter(Mandatory)][string]$ProjectRoot)
    $header = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src/launcher/version.h') -Raw
    $match = [regex]::Match($header, 'DISNEY_LAUNCHER_VERSION_STRING_UTF8\s+"([0-9]+\.[0-9]+\.[0-9]+)"')
    if (-not $match.Success) { throw 'Versao do launcher nao encontrada em src/launcher/version.h.' }
    return $match.Groups[1].Value
}

function Get-DisneyFileHash {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-DisneySourceState {
    param([Parameter(Mandatory)][string]$ProjectRoot)
    $commit = (& git -C $ProjectRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel identificar o commit do projeto.' }
    $status = ((& git -C $ProjectRoot status --porcelain=v1) -join "`n")
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel verificar alteracoes locais.' }
    $diff = (& git -C $ProjectRoot diff --binary HEAD --)
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel calcular o diff do projeto.' }
    $untracked = @(& git -C $ProjectRoot ls-files --others --exclude-standard)
    if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel identificar arquivos nao versionados.' }
    $untrackedHashes = foreach ($relative in $untracked) {
        "$relative`0$(Get-FileHash -LiteralPath (Join-Path $ProjectRoot $relative) -Algorithm SHA256).Hash.ToLowerInvariant()"
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($status + "`n" + ($diff -join "`n") + "`n" + ($untrackedHashes -join "`n"))
        $treeHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    } finally { $sha.Dispose() }
    return @{
        source_commit = $commit.Trim()
        source_dirty = if ([string]::IsNullOrWhiteSpace($status)) { 'false' } else { 'true' }
        source_state_sha256 = $treeHash
        launcher_version = Get-DisneyPackageVersion $ProjectRoot
    }
}

function Assert-DisneyFileHash {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$ExpectedSha256)
    if ($ExpectedSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw "SHA-256 invalido para $Path" }
    $actual = Get-DisneyFileHash $Path
    if ($actual -ne $ExpectedSha256.ToLowerInvariant()) {
        throw "SHA-256 divergente para $Path. Esperado $ExpectedSha256; obtido $actual."
    }
}

function Invoke-DisneyExecutable {
    param([Parameter(Mandatory)][string]$Path, [string[]]$ArgumentList = @())
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = (Resolve-Path -LiteralPath $Path).Path
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    if ($start.PSObject.Properties.Name -contains 'ArgumentList') {
        foreach ($argument in $ArgumentList) { $start.ArgumentList.Add($argument) }
    } else {
        $start.Arguments = ($ArgumentList | ForEach-Object {
            if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ }
        }) -join ' '
    }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    [void]$process.Start()
    $standardOutput = $process.StandardOutput.ReadToEndAsync()
    $standardError = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) {
        $process.Kill()
        throw "Tempo excedido ao executar $Path"
    }
    $process.WaitForExit()
    $result = [PSCustomObject]@{
        ExitCode = $process.ExitCode
        StandardOutput = $standardOutput.Result
        StandardError = $standardError.Result
    }
    $process.Dispose()
    return $result
}

function Expand-DisneyZip {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [string]$ExpectedSha256 = '',
        [string]$ProjectRoot = ''
    )
    $archivePath = $Source
    if ($Source -match '^https://') {
        if (-not $ExpectedSha256) { throw "Informe o SHA-256 para o download $Source" }
        if (-not $ProjectRoot) { throw 'ProjectRoot e obrigatorio para downloads de pacote.' }
        $inputDirectory = Join-Path $ProjectRoot 'build/package-inputs'
        New-Item -ItemType Directory -Force -Path $inputDirectory | Out-Null
        $archivePath = Join-Path $inputDirectory ([IO.Path]::GetFileName(([Uri]$Source).AbsolutePath))
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
            Invoke-WebRequest -Uri $Source -OutFile $archivePath
        }
    } else {
        $archivePath = (Resolve-Path -LiteralPath $Source).Path
    }
    if ($ExpectedSha256) { Assert-DisneyFileHash -Path $archivePath -ExpectedSha256 $ExpectedSha256 }
    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $destinationRoot = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
        foreach ($entry in $archive.Entries) {
            $relative = $entry.FullName.Replace('/', '\')
            $target = [IO.Path]::GetFullPath((Join-Path $Destination $relative))
            if (-not $target.StartsWith($destinationRoot, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Caminho inseguro no ZIP: $($entry.FullName)"
            }
            if ($entry.Name -eq '') {
                New-Item -ItemType Directory -Force -Path $target | Out-Null
            } else {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $true)
            }
        }
    } finally { $archive.Dispose() }
    return $archivePath
}

function Get-DisneyRedistDirectory {
    param([string]$ProjectRoot, [string]$ExplicitDirectory = '')
    if ($ExplicitDirectory) {
        $resolved = (Resolve-Path -LiteralPath $ExplicitDirectory).Path
        foreach ($name in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
            if (-not (Test-Path -LiteralPath (Join-Path $resolved $name) -PathType Leaf)) {
                throw "Redistribuivel ausente em $resolved`: $name"
            }
        }
        return $resolved
    }
    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            $versions = Join-Path $install 'VC\Redist\MSVC'
            if (Test-Path -LiteralPath $versions -PathType Container) {
                $latest = Get-ChildItem -LiteralPath $versions -Directory |
                    Where-Object { $_.Name -match '^[0-9]+(\.[0-9]+){2,3}$' } |
                    Sort-Object { [version]$_.Name } -Descending |
                    Select-Object -First 1
                if ($latest) {
                    $candidate = Join-Path $latest.FullName 'x64\Microsoft.VC143.CRT'
                    if (Test-Path -LiteralPath (Join-Path $candidate 'msvcp140.dll') -PathType Leaf) {
                        return $candidate
                    }
                }
            }
        }
    }
    foreach ($name in @('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $env:WINDIR "System32\$name") -PathType Leaf)) {
            throw "Microsoft Visual C++ Redistributable x64 nao encontrado: $name"
        }
    }
    return Join-Path $env:WINDIR 'System32'
}

function Copy-DisneyRuntime {
    param(
        [Parameter(Mandatory)][string]$SourceDirectory,
        [Parameter(Mandatory)][string]$DestinationDirectory,
        [Parameter(Mandatory)][string]$RedistDirectory
    )
    New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
    foreach ($name in $script:RuntimeFileNames) {
        $source = Join-Path $SourceDirectory $name
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            $source = Join-Path $RedistDirectory $name
        }
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Componente de runtime ausente: $name" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $DestinationDirectory $name) -Force
    }
}

function Find-DisneyNeversoft {
    param([Parameter(Mandatory)][string]$Directory)
    $tool = Get-ChildItem -LiteralPath $Directory -Recurse -File -Filter 'NeversoftMultitool.exe' |
        Select-Object -First 1
    if (-not $tool) { throw "NeversoftMultitool.exe ausente em $Directory" }
    return $tool.FullName
}

function Test-DisneyPeX64 {
    param([Parameter(Mandatory)][string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    try {
        $header = New-Object byte[] 64
        if ($stream.Read($header, 0, $header.Length) -ne $header.Length) { return $false }
        $peOffset = [BitConverter]::ToInt32($header, 0x3c)
        if ($peOffset -lt 0 -or $peOffset + 6 -gt $stream.Length) { return $false }
        $stream.Position = $peOffset
        $signature = New-Object byte[] 6
        if ($stream.Read($signature, 0, 6) -ne 6) { return $false }
        return $signature[0] -eq 0x50 -and $signature[1] -eq 0x45 -and
            $signature[2] -eq 0 -and $signature[3] -eq 0 -and
            [BitConverter]::ToUInt16($signature, 4) -eq 0x8664
    } finally { $stream.Dispose() }
}

function Write-DisneyPayloadHashes {
    param([Parameter(Mandatory)][string]$PackageRoot)
    $manifest = Join-Path $PackageRoot 'package-files.sha256'
    $lines = Get-ChildItem -LiteralPath $PackageRoot -Recurse -File |
        Where-Object { $_.FullName -ne $manifest } |
        Sort-Object FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($PackageRoot.Length).TrimStart('\').Replace('\', '/')
            "$(Get-DisneyFileHash $_.FullName)  $relative"
        }
    [IO.File]::WriteAllLines($manifest, $lines, (New-Object Text.UTF8Encoding($false)))
}

function Compress-DisneyPortable {
    param(
        [Parameter(Mandatory)][string]$PackageRoot,
        [Parameter(Mandatory)][string]$OutputZip,
        [Parameter(Mandatory)][string]$Version
    )
    if (Test-Path -LiteralPath $OutputZip) { Remove-Item -LiteralPath $OutputZip -Force }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $prefix = "DisneySkate-$Version/"
    $archive = [IO.Compression.ZipFile]::Open($OutputZip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem -LiteralPath $PackageRoot -Recurse -File | Sort-Object FullName) {
            $relative = $file.FullName.Substring($PackageRoot.Length).TrimStart('\').Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $prefix + $relative,
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        $archive.CreateEntry($prefix + 'launcher.portable') | Out-Null
    } finally { $archive.Dispose() }
}

Export-ModuleMember -Function Get-DisneyRuntimeFileNames, Get-DisneyPackageVersion, Get-DisneySourceState, Get-DisneyFileHash, Assert-DisneyFileHash, Invoke-DisneyExecutable,
    Expand-DisneyZip, Get-DisneyRedistDirectory, Copy-DisneyRuntime, Find-DisneyNeversoft,
    Test-DisneyPeX64, Write-DisneyPayloadHashes, Compress-DisneyPortable
