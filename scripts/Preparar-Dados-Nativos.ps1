param([switch]$ExtrairNovamente)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$tool=Get-ChildItem -LiteralPath "$projectRoot\tools\neversoft-multitool-v1.3.4\cli" -Recurse -Filter 'NeversoftMultitool.exe' | Select-Object -First 1 -ExpandProperty FullName
if(-not $tool){throw 'Neversoft Multitool CLI v1.3.4 ausente; veja docs/teste-nativo.md'}
$extracted="$projectRoot\original\extracted"
$source="$projectRoot\assets\source\wad\SKATE4\pre"
function Invoke-Conversion {
    param([string]$Log,[string[]]$ToolArgs,[string]$Expected)
    & $tool @ToolArgs *> "$projectRoot\analysis\$Log"
    if($LASTEXITCODE -ne 0){throw "Conversao falhou: $Log"}
    if($Expected -and -not(Test-Path -LiteralPath $Expected)){throw "Saida nao produzida: $Expected. Confira $Log"}
}
if($ExtrairNovamente -or -not(Test-Path -LiteralPath "$source\HUBScn\Levels\HUB\HUB.geom.ps2")){
    Invoke-Conversion 'neversoft-wad-extraction.log' @('archive',"$extracted\SKATE4.WAD",'-o',"$projectRoot\assets\source\wad") "$source\HUBScn.pre"
    Invoke-Conversion 'neversoft-pre-extraction.log' @('unpack',"$projectRoot\assets\source\wad") "$source\HUBScn\Levels\HUB\HUB.geom.ps2"
}
Invoke-Conversion 'neversoft-timon-conversion.log' @('mesh',"$source\SkateShopPed\models\timon\timon.skin.ps2",'-o',"$projectRoot\assets\native\characters\timon",'--tex',"$source\SkateShopPed\models\timon\timon.tex.ps2",'--ske',"$source\skeletons\skeletons\timon.ske") "$projectRoot\assets\native\characters\timon\timon.glb"
Invoke-Conversion 'neversoft-hub-conversion.log' @('mesh',"$source\HUBScn\Levels\HUB\HUB.geom.ps2",'-o',"$projectRoot\assets\native\levels\hub",'--tex',"$source\HUBScn\Levels\HUB\HUB.tex.ps2") "$projectRoot\assets\native\levels\hub\HUB.glb"
Invoke-Conversion 'neversoft-hub-script.log' @('qb',"$source\HUB\levels\Hub\HUB.qb",'-o',"$projectRoot\assets\native\scripts") "$projectRoot\assets\native\scripts\HUB.q"
Invoke-Conversion 'neversoft-physics-decompilation.log' @('qb',"$source\qb\scripts\PHYSICS.qb",'-o',"$projectRoot\assets\native\scripts") "$projectRoot\assets\native\scripts\PHYSICS.q"
$sourceText=Get-Content -LiteralPath "$projectRoot\assets\native\scripts\HUB.q" -Raw
$match=[regex]::Match($sourceText,'Position = \(([-\d.]+), ([-\d.]+), ([-\d.]+)\) Angles = \(0, ([-\d.]+), 0\) Name = TRG_Start_P1 ')
if(-not $match.Success){throw 'Ponto TRG_Start_P1 nao recuperado do HUB'}
$culture=[Globalization.CultureInfo]::InvariantCulture
$values=1..4 | ForEach-Object{[float]::Parse($match.Groups[$_].Value,$culture)}
# QB level coordinates mirror Z relative to the render/collision meshes.
$spawn=@($values[0],$values[1],-$values[2],([Math]::PI-$values[3])) | ForEach-Object{$_.ToString('R',$culture)}
[IO.File]::WriteAllText("$projectRoot\assets\native\spawn.txt",($spawn -join ' '))
$converter="$projectRoot\tools-src\col8-converter"
$oldDotnetHome=$env:DOTNET_CLI_HOME
try {
    $env:DOTNET_CLI_HOME="$projectRoot\build\dotnet-home"
    & dotnet build "$converter\Col8Converter.csproj" -c Release *> "$projectRoot\analysis\col8-build.log"
    if($LASTEXITCODE -ne 0){throw 'Falha ao compilar conversor COL v8; requer SDK .NET 9'}
    & dotnet "$converter\bin\Release\net9.0\Col8Converter.dll" "$source\HUBcol\Levels\HUB\HUB.col.ps2" "$projectRoot\assets\native\collision\hub\HUB.dscollision" *> "$projectRoot\analysis\col8-conversion.log"
    if($LASTEXITCODE -ne 0){throw 'Falha ao converter colisoes COL v8'}
} finally {$env:DOTNET_CLI_HOME=$oldDotnetHome}
Write-Output 'Dados nativos do HUB e Timon preparados. Nenhum executavel ou modulo do PS2 foi executado.'
