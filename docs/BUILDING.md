# Compilação

O build padrão compila o launcher oficial (`src/launcher`), o conversor de assets e o núcleo
nativo que está versionado. Ele não compila o runtime completo do jogo. Uma compilação deste
build padrão passou em uma cópia isolada contendo somente arquivos rastreados do Git, usando
Windows 11, Visual Studio 2022 Build Tools e CMake 3.31 do Visual Studio. O workflow
`.github/workflows/windows-source-build.yml` repete essa compilação em Windows hospedado pelo
GitHub Actions quando o workflow for executado.

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
recompilação do código do jogo, mas ainda não há um fluxo headless configurado e validado no
repositório.

## Build padrão do código versionado

No PowerShell, a partir da raiz do projeto (com CMake e MSVC disponíveis):

```powershell
cmake -S . -B build/source -A x64
cmake --build build/source --config Release --parallel 2
```

O resultado inclui `Disney's Extreme Skate Adventure.exe`, que é o launcher oficial do
repositório. Sem os dados locais, o launcher apenas informa que o pacote do jogo não está
preparado.

## Runtime AOT (ainda dependente do ambiente local)

O comando abaixo é o fluxo antigo usado pelo ambiente de desenvolvimento e pode falhar em clone
limpo:

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

Ele espera PS2Recomp, a biblioteca IOP compilada e fontes recompiladas de EE/VU. O projeto ainda
precisa versionar ou obter de forma determinística as ferramentas permitidas e fornecer um
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
