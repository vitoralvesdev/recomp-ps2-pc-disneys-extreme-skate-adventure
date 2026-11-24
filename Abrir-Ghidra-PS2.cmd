@echo off
setlocal

if not "%~1"=="" set "GHIDRA_HOME=%~1"

if defined JAVA_HOME (
    if not exist "%JAVA_HOME%\bin\java.exe" (
        echo ERRO: JAVA_HOME nao aponta para um JDK valido: "%JAVA_HOME%"
        exit /b 1
    )
    set "PATH=%JAVA_HOME%\bin;%PATH%"
)

if defined GHIDRA_HOME (
    if not exist "%GHIDRA_HOME%\ghidraRun.bat" (
        echo ERRO: GHIDRA_HOME nao contem ghidraRun.bat: "%GHIDRA_HOME%"
        exit /b 1
    )
    call "%GHIDRA_HOME%\ghidraRun.bat"
    exit /b
)

set "GHIDRA_RUN="
for /f "delims=" %%G in ('where ghidraRun.bat 2^>nul') do (
    if not defined GHIDRA_RUN set "GHIDRA_RUN=%%G"
)
if not defined GHIDRA_RUN (
    echo ERRO: Ghidra nao encontrado no PATH.
    echo Defina GHIDRA_HOME ou informe o diretorio do Ghidra como primeiro argumento.
    exit /b 1
)

call "%GHIDRA_RUN%"
exit /b
