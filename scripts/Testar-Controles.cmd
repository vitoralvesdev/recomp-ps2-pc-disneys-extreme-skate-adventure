@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /std:c++20 /I tools\PS2Recomp\ps2xRuntime\include /I C:\Users\vitor\OneDrive\Documentos\ChatGPT\REA\deps-ps2\raylib-src\src tests\pc_pad_test.cpp src\host\pc_pad.cpp /Fe:build\pc_pad_test.exe /Fo:build\
if errorlevel 1 exit /b 1
build\pc_pad_test.exe
