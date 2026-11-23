Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$projectRoot = Split-Path $PSScriptRoot -Parent
$form = New-Object System.Windows.Forms.Form
$form.Text = "Instalar Disney's Extreme Skate Adventure"
$form.StartPosition = 'CenterScreen'
$form.Size = New-Object System.Drawing.Size(680, 460)
$form.MinimumSize = New-Object System.Drawing.Size(680, 460)

$intro = New-Object System.Windows.Forms.Label
$intro.Text = "Selecione a ISO original USA de PS2. O assistente extrai os arquivos localmente e tenta preparar e compilar o projeto. A ISO e os dados do jogo permanecem neste computador."
$intro.Location = New-Object System.Drawing.Point(16, 14)
$intro.Size = New-Object System.Drawing.Size(630, 48)
$intro.Anchor = 'Top,Left,Right'
$form.Controls.Add($intro)

$isoBox = New-Object System.Windows.Forms.TextBox
$isoBox.Location = New-Object System.Drawing.Point(16, 72)
$isoBox.Size = New-Object System.Drawing.Size(520, 24)
$isoBox.Anchor = 'Top,Left,Right'
$form.Controls.Add($isoBox)

$browse = New-Object System.Windows.Forms.Button
$browse.Text = 'Selecionar ISO...'
$browse.Location = New-Object System.Drawing.Point(544, 70)
$browse.Size = New-Object System.Drawing.Size(102, 28)
$browse.Anchor = 'Top,Right'
$form.Controls.Add($browse)

$log = New-Object System.Windows.Forms.TextBox
$log.Location = New-Object System.Drawing.Point(16, 110)
$log.Size = New-Object System.Drawing.Size(630, 260)
$log.Multiline = $true
$log.ReadOnly = $true
$log.ScrollBars = 'Vertical'
$log.Anchor = 'Top,Bottom,Left,Right'
$form.Controls.Add($log)

$install = New-Object System.Windows.Forms.Button
$install.Text = 'Preparar instalação'
$install.Location = New-Object System.Drawing.Point(496, 380)
$install.Size = New-Object System.Drawing.Size(150, 30)
$install.Anchor = 'Bottom,Right'
$form.Controls.Add($install)

function Add-Log([string]$Message) {
    $log.AppendText("$Message`r`n")
    $log.SelectionStart = $log.TextLength
    $log.ScrollToCaret()
    [System.Windows.Forms.Application]::DoEvents()
}

$browse.Add_Click({
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = 'Selecione a imagem USA do jogo'
    $dialog.Filter = 'Imagem ISO (*.iso)|*.iso|Todos os arquivos (*.*)|*.*'
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $isoBox.Text = $dialog.FileName }
    $dialog.Dispose()
})

$install.Add_Click({
    $iso = $isoBox.Text.Trim()
    if (-not (Test-Path -LiteralPath $iso -PathType Leaf)) {
        [System.Windows.Forms.MessageBox]::Show('Selecione um arquivo de imagem de disco existente.', $form.Text, 'OK', 'Warning') | Out-Null
        return
    }

    $sevenZip = Join-Path $projectRoot 'tools\7-Zip\7z.exe'
    if (-not (Test-Path -LiteralPath $sevenZip)) {
        $command = Get-Command 7z.exe -ErrorAction SilentlyContinue
        if ($command) { $sevenZip = $command.Source }
    }

    $missingPrerequisites = [System.Collections.Generic.List[string]]::new()
    if (-not (Test-Path -LiteralPath $sevenZip -PathType Leaf)) {
        $missingPrerequisites.Add('7-Zip (7z.exe) não encontrado no PATH nem em tools\7-Zip.')
    }
    $multitool = Get-ChildItem -LiteralPath (Join-Path $projectRoot 'tools\neversoft-multitool-v1.3.4\cli') -Recurse -File -Filter 'NeversoftMultitool.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $multitool) {
        $missingPrerequisites.Add('Neversoft Multitool CLI v1.3.4 ausente em tools\neversoft-multitool-v1.3.4\cli.')
    }
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'assets\native\audio\catalog.tsv') -PathType Leaf)) {
        $missingPrerequisites.Add('Catálogo de áudio ausente: assets\native\audio\catalog.tsv.')
    }
    $requiredGeneratedSources = @(
        'src\generated-ghidra\FUN_002d2b90_0x2d2b90.cpp',
        'src\generated-ghidra\FUN_002d3848_0x2d3848.cpp',
        'src\generated-ghidra\FUN_002d3888_0x2d3888.cpp'
    )
    $missingGeneratedSources = @($requiredGeneratedSources | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $projectRoot $_) -PathType Leaf)
    })
    if ($missingGeneratedSources.Count -gt 0 -or
        -not (Get-ChildItem -LiteralPath (Join-Path $projectRoot 'native\original\vu\generated') -Filter '*.cpp' -File -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        $missingPrerequisites.Add('Código AOT gerado ausente ou incompleto em src\generated-ghidra e native\original\vu\generated.')
    }
    $dotnet = Get-Command dotnet.exe -ErrorAction SilentlyContinue
    $hasDotnet9 = $false
    if ($dotnet) {
        $installedSdks = & $dotnet.Source --list-sdks 2>$null
        $hasDotnet9 = ($LASTEXITCODE -eq 0) -and [bool]($installedSdks | Where-Object { $_ -match '^9\.' })
    }
    if (-not $hasDotnet9) {
        $missingPrerequisites.Add('SDK .NET 9 necessário para compilar o conversor de colisão não encontrado.')
    }
    try {
        Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1') -ErrorAction Stop
        $null = Get-DisneyCMake
    } catch {
        $missingPrerequisites.Add('CMake 3.21+ não encontrado.')
    }
    if ($missingPrerequisites.Count -gt 0) {
        $message = "A instalação não foi iniciada. Corrija estes pré-requisitos:`n`n- " + ($missingPrerequisites -join "`n- ")
        [System.Windows.Forms.MessageBox]::Show($message, $form.Text, 'OK', 'Error') | Out-Null
        foreach ($item in $missingPrerequisites) { Add-Log "Pré-requisito ausente: $item" }
        return
    }

    $install.Enabled = $false
    $browse.Enabled = $false
    $isoBox.Enabled = $false
    $staging = Join-Path $projectRoot ('build\install-staging-' + [guid]::NewGuid().ToString('N'))
    try {
        Add-Log 'Extraindo a imagem de disco para uma pasta temporária...'
        New-Item -ItemType Directory -Force -Path $staging | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'analysis') | Out-Null
        & $sevenZip x '-y' "-o$staging" $iso *> (Join-Path $projectRoot 'analysis\installer-7zip.log')
        if ($LASTEXITCODE -ne 0) { throw "A extração falhou (código $LASTEXITCODE). Consulte analysis\installer-7zip.log." }

        Add-Log 'Validando a edição USA e os arquivos necessários...'
        $expected = @('SLUS_206.07', 'SKATE4.HED', 'SKATE4.WAD', 'SIO2MAN.IRX', 'PADMAN.IRX', 'LIBSD.IRX', 'SDRDRV.IRX', 'MCMAN.IRX', 'MCSERV.IRX', 'EZPCM.IRX')
        $found = @{}
        foreach ($file in Get-ChildItem -LiteralPath $staging -Recurse -File) {
            if ($expected -contains $file.Name) { $found[$file.Name] = $file.FullName }
        }
        $missing = @($expected | Where-Object { -not $found.ContainsKey($_) })
        if ($missing.Count) { throw "A imagem não parece ser a edição USA esperada ou faltam arquivos: $($missing -join ', '). Nenhum dado foi instalado." }

        $exeDir = Split-Path $found['SLUS_206.07'] -Parent
        $dest = Join-Path $projectRoot 'original\extracted'
        Add-Log 'Copiando os arquivos originais para original\extracted...'
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Copy-Item -Path (Join-Path $exeDir '*') -Destination $dest -Recurse -Force
        foreach ($file in @('SKATE4.HED', 'SKATE4.WAD')) {
            Copy-Item -LiteralPath $found[$file] -Destination (Join-Path $dest $file) -Force
        }
        $iopDest = Join-Path $dest 'IOP'
        New-Item -ItemType Directory -Force -Path $iopDest | Out-Null
        foreach ($file in @('SIO2MAN.IRX', 'PADMAN.IRX', 'LIBSD.IRX', 'SDRDRV.IRX', 'MCMAN.IRX', 'MCSERV.IRX', 'EZPCM.IRX')) {
            Copy-Item -LiteralPath $found[$file] -Destination (Join-Path $iopDest $file) -Force
        }

        Add-Log 'Preparando os assets nativos...'
        & (Join-Path $PSScriptRoot 'Preparar-Dados-Nativos.ps1')
        if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'A preparação dos assets nativos falhou.' }

        Add-Log 'Compilando o runtime e o launcher...'
        & (Join-Path $PSScriptRoot 'Compilar-Candidato-Original.ps1')
        if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'A compilação do runtime/launcher falhou.' }

        $launcher = Join-Path $projectRoot "Disney's Extreme Skate Adventure.exe"
        if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw 'O build terminou sem produzir o launcher.' }
        Add-Log 'Verificando os arquivos exigidos pelo launcher...'
        & $launcher --verificar *> (Join-Path $projectRoot 'analysis\installer-package-check.log')
        if ($LASTEXITCODE -ne 0) {
            throw 'O launcher detectou arquivos ausentes no pacote. Confira analysis\installer-package-check.log.'
        }

        Add-Log 'Pacote validado. Abra Disney''s Extreme Skate Adventure.exe na pasta do projeto.'
        [System.Windows.Forms.MessageBox]::Show("Preparação concluída e pacote verificado.`n`nAbra o launcher na pasta do projeto.", $form.Text, 'OK', 'Information') | Out-Null
    } catch {
        Add-Log "ERRO: $($_.Exception.Message)"
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, $form.Text, 'OK', 'Error') | Out-Null
    } finally {
        if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
        $install.Enabled = $true
        $browse.Enabled = $true
        $isoBox.Enabled = $true
    }
})

[void]$form.ShowDialog()
$form.Dispose()
