param([Parameter(Mandatory=$true)][string]$ElfPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$env:JAVA_HOME = 'C:\Program Files\Java\jdk-23'
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
$headless = 'C:\Users\vitor\OneDrive\Documentos\ChatGPT\REA\tools\ghidra-12.1.4\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat'
if (!(Test-Path -LiteralPath $ElfPath -PathType Leaf)) { throw 'ELF nao encontrado' }
& $headless "$projectRoot\analysis" DisneySkate -import $ElfPath -log "$projectRoot\analysis\ghidra.log"
if ($LASTEXITCODE -ne 0) { throw 'Falha na analise Ghidra; consulte o log' }

