# Compilação

O launcher oficial (`src/launcher`) tem um alvo isolado que compila sem ISO, raylib, PS2Recomp,
fontes geradas ou dados do jogo. Uma compilação desse alvo passou em uma cópia isolada contendo
somente arquivos rastreados do Git, usando Windows 11, Visual Studio 2022 Build Tools e CMake 3.31
do Visual Studio. O workflow `.github/workflows/windows-source-build.yml` repete esse build e os
testes em Windows hospedado pelo GitHub Actions.

O build completo do código versionado (launcher + conversor + núcleo nativo) também existe, mas
não compila o runtime AOT do jogo.

O runtime completo ainda **não é reproduzível a partir de um clone limpo**: o candidato AOT
depende de PS2Recomp e bibliotecas que não estão versionados, além de código gerado a partir da
ISO em diretórios ignorados (`src/generated-*`, `native/original/vu/generated/` e
`native/original/iop/generated/`). Esta página não declara o build do jogo concluído.

## Pré-requisitos

- Windows 10/11 x64.
- Visual Studio Build Tools com **Desktop development with C++** e MSVC x64.
- CMake 3.21 ou superior (o Visual Studio inclui CMake; `cmake` também pode estar no `PATH`).
- Uma cópia legal do jogo somente para preparar/compilar o runtime completo.

Para o conversor de colisão, também é necessário .NET SDK 9. Ghidra é usado na análise e
recompilação do código do jogo. `Abrir-Ghidra-PS2.cmd` aceita `GHIDRA_HOME` ou o diretório do
Ghidra como primeiro argumento, descobre `ghidraRun.bat` no `PATH` e respeita `JAVA_HOME`. O fluxo
headless ainda não está validado no repositório.

## Launcher (sem dados do jogo)

Recomendado para desenvolver a interface e o instalador:

```powershell
.\scripts\Compilar-Launcher.ps1
```

Ou manualmente:

```powershell
cmake -S . -B build/launcher -G "Visual Studio 17 2022" -A x64 -DDISNEY_BUILD_LAUNCHER_ONLY=ON
cmake --build build/launcher --config Release --target DisneySkateLauncher DisneyLauncherTests --parallel 2
ctest --test-dir build/launcher -C Release --output-on-failure
```

O resultado é `Disney's Extreme Skate Adventure.exe` na raiz. Sem os dados locais, o launcher
abre a tela de instalação e mostra quais componentes faltam.

Os testes não precisam de ISO. Para a integração real:

```powershell
.\scripts\Compilar-Launcher.ps1 -Iso "C:\Jogos\Disney's Extreme Skate Adventure (USA).iso"
```

Os testes verificam ISO9660, validação de região/serial, hash do ELF, extração, configuração,
detecção de layout, verificação de pacote e processos. Eles nunca executam código do jogo.

O código-fonte é UTF-8; o CMake aplica `/utf-8` ao MSVC para evitar mojibake nos textos.

## Build completo do código versionado

Este build inclui o conversor de assets e o núcleo nativo versionado:

```powershell
cmake -S . -B build/source -A x64
cmake --build build/source --config Release --parallel 2
```

Sem os dados locais, o launcher apenas informa que o pacote do jogo não está preparado. O
laboratório de assets opcional (`DISNEY_BUILD_RESOURCE_LAB=ON`) exige raylib 5.5 e usa
`DISNEY_RAYLIB_ROOT` para localizar `raylib-src` e `raylib-build`, sem caminho pessoal padrão.

## Runtime AOT (ainda dependente do ambiente local)

O comando abaixo é o fluxo antigo usado pelo ambiente de desenvolvimento e pode falhar em clone
limpo:

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

Ele espera PS2Recomp, a biblioteca IOP compilada e fontes recompiladas de EE/VU. O script passa o
caminho efetivamente compilado em `DISNEY_ORIGINAL_IOP_LIBRARY`, e o CMake preserva esse parâmetro;
uma configuração manual deve usar a mesma variável. O projeto ainda precisa versionar ou obter de
forma determinística as ferramentas permitidas e fornecer um
gerador automatizado para as saídas derivadas da ISO antes que esse alvo possa ser considerado
reproduzível. Não publique `original/`, `assets/`, `runtime/`, `build/`, `analysis/` ou `dist/`:
veja [DISTRIBUTION.md](DISTRIBUTION.md).

## Preparar dados locais

Com os dados extraídos já presentes em `original/extracted/`, use:

```powershell
.\scripts\Preparar-Dados-Nativos.ps1
```

Esse script transforma dados locais para testes. Ele não baixa uma ISO nem substitui a cópia
legal do jogador.
