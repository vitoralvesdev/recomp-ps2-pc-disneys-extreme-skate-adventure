param(
    [switch]$ExtrairNovamente,
    [string]$GameDir = '',
    [string]$AssetsDir = '',
    [string]$AnalysisDir = '',
    [string]$BuildDir = '',
    [string]$ToolsDir = '',
    [string]$ConverterDir = ''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(-not $GameDir){$GameDir=Join-Path $projectRoot 'original/extracted'}
if(-not $AssetsDir){$AssetsDir=Join-Path $projectRoot 'assets'}
if(-not $AnalysisDir){$AnalysisDir=Join-Path $projectRoot 'analysis'}
if(-not $BuildDir){$BuildDir=Join-Path $projectRoot 'build'}
if(-not $ToolsDir){$ToolsDir=Join-Path $projectRoot 'tools'}
if(-not $ConverterDir){$ConverterDir=Join-Path $projectRoot 'tools-src/col8-converter'}
$GameDir=[IO.Path]::GetFullPath($GameDir)
$AssetsDir=[IO.Path]::GetFullPath($AssetsDir)
$AnalysisDir=[IO.Path]::GetFullPath($AnalysisDir)
$BuildDir=[IO.Path]::GetFullPath($BuildDir)
$native=Join-Path $AssetsDir 'native'
$wadRoot=Join-Path $AssetsDir 'source/wad'
$source=Join-Path $wadRoot 'SKATE4/pre'
New-Item -ItemType Directory -Force -Path $native,$wadRoot,$AnalysisDir,$BuildDir | Out-Null
Import-Module (Join-Path $PSScriptRoot 'Build.Common.psm1')
$toolRoot=Join-Path $ToolsDir 'neversoft-multitool-v1.3.4/cli'
if(-not (Test-Path -LiteralPath $toolRoot -PathType Container)){throw "Neversoft Multitool CLI v1.3.4 ausente em $ToolsDir"}
$tool=Get-ChildItem -LiteralPath $toolRoot -Recurse -Filter 'NeversoftMultitool.exe' | Select-Object -First 1 -ExpandProperty FullName
if(-not $tool){throw 'Neversoft Multitool CLI v1.3.4 ausente; veja docs/teste-nativo.md'}
function Invoke-Conversion {
    param([string]$Log,[string[]]$ToolArgs,[string]$Expected,[int]$Percent,[string]$Message)
    Write-DisneyProgress $Percent $Message
    & $tool @ToolArgs *> (Join-Path $AnalysisDir $Log)
    if($LASTEXITCODE -ne 0){throw "Conversao falhou: $Log"}
    if($Expected -and -not(Test-Path -LiteralPath $Expected)){throw "Saida nao produzida: $Expected. Confira $Log"}
}
Write-DisneyProgress 5 'Convertendo os dados originais extraidos...'
if($ExtrairNovamente -or -not(Test-Path -LiteralPath "$source\HUBScn\Levels\HUB\HUB.geom.ps2")){
    Invoke-Conversion 'neversoft-wad-extraction.log' @('archive',(Join-Path $GameDir 'SKATE4.WAD'),'-o',$wadRoot) "$source\HUBScn.pre" 10 'Extraindo SKATE4.WAD...'
    Invoke-Conversion 'neversoft-pre-extraction.log' @('unpack',$wadRoot) "$source\HUBScn\Levels\HUB\HUB.geom.ps2" 20 'Descompactando os recursos das fases...'
}
Invoke-Conversion 'neversoft-timon-conversion.log' @('mesh',"$source\SkateShopPed\models\timon\timon.skin.ps2",'-o',"$native\characters\timon",'--tex',"$source\SkateShopPed\models\timon\timon.tex.ps2",'--ske',"$source\skeletons\skeletons\timon.ske") "$native\characters\timon\timon.glb" 30 'Convertendo personagens e texturas...'
Invoke-Conversion 'neversoft-hub-conversion.log' @('mesh',"$source\HUBScn\Levels\HUB\HUB.geom.ps2",'-o',"$native\levels\hub",'--tex',"$source\HUBScn\Levels\HUB\HUB.tex.ps2") "$native\levels\hub\HUB.glb" 40 'Convertendo geometrias e materiais...'
Invoke-Conversion 'neversoft-hub-script.log' @('qb',"$source\HUB\levels\Hub\HUB.qb",'-o',"$native\scripts") "$native\scripts\HUB.q" 50 'Convertendo scripts da fase...'
Invoke-Conversion 'neversoft-physics-decompilation.log' @('qb',"$source\qb\scripts\PHYSICS.qb",'-o',"$native\scripts") "$native\scripts\PHYSICS.q" 55 'Convertendo regras originais de fisica...'
$sourceText=Get-Content -LiteralPath "$native\scripts\HUB.q" -Raw
$match=[regex]::Match($sourceText,'Position = \(([-\d.]+), ([-\d.]+), ([-\d.]+)\) Angles = \(0, ([-\d.]+), 0\) Name = TRG_Start_P1 ')
if(-not $match.Success){throw 'Ponto TRG_Start_P1 nao recuperado do HUB'}
$culture=[Globalization.CultureInfo]::InvariantCulture
$values=1..4 | ForEach-Object{[float]::Parse($match.Groups[$_].Value,$culture)}
# QB level coordinates mirror Z relative to the render/collision meshes.
$spawn=@($values[0],$values[1],-$values[2],([Math]::PI-$values[3])) | ForEach-Object{$_.ToString('R',$culture)}
[IO.File]::WriteAllText("$native\spawn.txt",($spawn -join ' '))
$oldDotnetHome=$env:DOTNET_CLI_HOME
try {
    $env:DOTNET_CLI_HOME=Join-Path $BuildDir 'dotnet-home'
    Write-DisneyProgress 65 'Compilando o conversor de colisoes COL v8...'
    & dotnet build $ConverterDir\Col8Converter.csproj -c Release *> (Join-Path $AnalysisDir 'col8-build.log')
    if($LASTEXITCODE -ne 0){throw 'Falha ao compilar conversor COL v8; requer SDK .NET 9'}
    $collisionSources=Get-ChildItem -LiteralPath $source -Recurse -File -Filter '*.col.ps2' | Sort-Object FullName
    if(-not $collisionSources){throw 'Nenhum arquivo COL de colisao foi encontrado'}
    $conversionLog=Join-Path $AnalysisDir 'col8-conversion.log'
    $converted=0
    foreach($collisionSource in $collisionSources){
        $relativeDirectory=$collisionSource.DirectoryName.Substring($source.Length).TrimStart('\')
        $levelName=[IO.Path]::GetFileNameWithoutExtension([IO.Path]::GetFileNameWithoutExtension($collisionSource.Name))
        $destination=Join-Path (Join-Path $native 'collision') (Join-Path $relativeDirectory "$levelName.dscollision")
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        & dotnet "$ConverterDir\bin\Release\net9.0\Col8Converter.dll" $collisionSource.FullName $destination *>> $conversionLog
        if($LASTEXITCODE -ne 0){throw "Falha ao converter colisao: $($collisionSource.FullName). Confira $conversionLog"}
        ++$converted
        if($converted % 10 -eq 0){Write-DisneyProgress (70 + [Math]::Min(20,[int](70*$converted/$collisionSources.Count))) "Colisoes convertidas: $converted/$($collisionSources.Count)"}
    }
} finally {$env:DOTNET_CLI_HOME=$oldDotnetHome}
Write-DisneyProgress 100 'Dados nativos das fases e Timon preparados.'
Write-Output 'Dados nativos das fases e Timon preparados. Nenhum executavel ou modulo do PS2 foi executado.'
