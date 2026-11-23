function Get-DisneyCMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
    if ($programFilesX86) {
        $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
            $install = & $vswhere -latest -products '*' -property installationPath
            if ($install) {
                $bundled = Join-Path $install 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
                if (Test-Path -LiteralPath $bundled -PathType Leaf) { return $bundled }
            }
        }
    }

    throw 'CMake 3.21+ não foi encontrado. Instale o CMake ou o componente CMake do Visual Studio e tente novamente.'
}

Export-ModuleMember -Function Get-DisneyCMake
