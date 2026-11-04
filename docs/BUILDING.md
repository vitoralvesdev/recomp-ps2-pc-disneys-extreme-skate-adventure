# Compilação local

Este guia é apenas para desenvolvimento. Jogadores devem usar o executável da raiz do pacote e não precisam instalar estas ferramentas.

## Pré-requisitos

- Windows 10/11 x64.
- Visual Studio Build Tools com **Desktop development with C++** e MSVC x64.
- CMake 3.21 ou superior.
- .NET SDK 9 apenas ao regenerar os dados nativos de colisão.
- Uma cópia legal e extraída do jogo USA (SLUS_206.07), mantida fora do Git.

O projeto usa cópias locais de CMake e PS2Recomp para a pesquisa de recompilação. Ghidra é necessário somente para análise de código, não para compilar um runtime já configurado.

## Compilar

No PowerShell, a partir da raiz do projeto:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\Compilar-Candidato-Original.ps1
```

Depois da compilação, abra `Disney's Extreme Skate Adventure.exe`. Não publique `original/`, `assets/`, `runtime/`, `build/`, `analysis/` ou `dist/`: a política de distribuição está em [DISTRIBUTION.md](DISTRIBUTION.md).

## Preparar dados locais

Com os dados extraídos já presentes em `original/extracted/`, use:

```powershell
.\scripts\Preparar-Dados-Nativos.ps1
```

Esse script transforma apenas dados locais para testes. Ele não concede licença, não baixa uma ISO e não deve ser usado para publicar recursos do jogo.
