# Compilação local

Este guia é apenas para desenvolvimento. O repositório não inclui um pacote pronto para jogar, as ferramentas locais ou os dados originais. A preparação completa a partir de um clone limpo ainda não foi validada.

## Pré-requisitos

- Windows 10/11 x64.
- Visual Studio Build Tools com **Desktop development with C++** e MSVC x64.
- CMake 3.21 ou superior.
- .NET SDK 9 apenas ao regenerar os dados nativos de colisão.
- Uma cópia legal e extraída do jogo USA (SLUS_206.07), mantida fora do Git.

O script atual depende de cópias locais de CMake e PS2Recomp, dependências previamente preparadas e código gerado de EE, VU e IOP que não é incluído no Git. Não há ainda instruções validadas para gerar tudo isso a partir de um clone novo. Ghidra é usado na análise de código.

## Compilar

No PowerShell, a partir da raiz do projeto:

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

Esse comando é o utilizado no ambiente de desenvolvimento existente; pode falhar em um clone limpo enquanto os pré-requisitos e a geração dos arquivos não estiverem completos. Depois de uma compilação e preparação bem-sucedidas, abra `Disney's Extreme Skate Adventure.exe`. Não publique `original/`, `assets/`, `runtime/`, `build/`, `analysis/` ou `dist/`: veja [DISTRIBUTION.md](DISTRIBUTION.md).

## Preparar dados locais

Com os dados extraídos já presentes em `original/extracted/`, use:

```powershell
.\scripts\Preparar-Dados-Nativos.ps1
```

Esse script transforma apenas dados locais para testes. Ele não concede licença, não baixa uma ISO e não deve ser usado para publicar recursos do jogo.
