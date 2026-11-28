# Launcher nativo

O launcher é o executável `Disney's Extreme Skate Adventure.exe` produzido a partir de
`src/launcher`. Ele é uma aplicação Win32 nativa em C++20, com interface GDI+. A seleção da ISO,
a validação, a extração e a interface não dependem de Electron, 7-Zip ou navegador; a preparação
de assets em um checkout de desenvolvimento usa os scripts PowerShell descritos abaixo.

Versão atual: **0.5.0**.

## O que ele faz

1. Identifica uma imagem de disco do PlayStation 2.
2. Valida a edição NTSC-U esperada: `SYSTEM.CNF` com `BOOT2` exatamente igual a
   `cdrom0:\SLUS_206.07;1`, `VMODE = NTSC`, ELF MIPS little-endian com o SHA-256 de referência
   e arquivos `SKATE4.HED`, `SKATE4.WAD` e módulos IOP não vazios.
3. Lê a ISO9660 diretamente (imagens de 2048 bytes por setor e variantes cruas de 2352/2336)
   e extrai os arquivos para a pasta de dados. Não é necessário instalar o 7-Zip.
4. Prepara os assets nativos e o runtime. Em um checkout de desenvolvimento, delega aos scripts
   existentes do projeto; em um pacote pré-compilado, valida os componentes já presentes.
5. Verifica os componentes necessários antes de liberar o botão **Jogar**. A auditoria integral do
   manifesto SHA-256 é executada na instalação e pelo comando `--verificar`.
6. Abre o runtime, configura o renderer, a escala, o V-Sync, a pasta de saves e os diagnósticos.
7. Mantém o launcher aberto enquanto o jogo roda e volta ao estado pronto quando o jogo fecha.

A extração é incremental: arquivos já presentes só são ignorados quando tamanho e SHA-256
coincidem com a ISO. Alterações no launcher não exigem reinstalar a ISO. Uma atualização do port
também pode substituir o launcher e o runtime sem repetir a extração; depois de substituir o
runtime ou os assets, execute `--atualizar-manifesto` para sincronizar a auditoria integral. A ISO
só é necessária novamente ao instalar ou restaurar os dados extraídos.

## Primeira execução

Na primeira vez, a tela inicial mostra o estado de cada componente e o botão **Instalar jogo**.

1. Clique em **Selecionar ISO** e escolha a imagem americana de PlayStation 2.
2. O launcher valida a imagem antes de escrever qualquer arquivo.
3. Clique em **Instalar jogo**. A tela de progresso mostra cinco etapas reais:
   verificação da imagem, extração, assets, runtime e verificação do pacote.
4. Ao terminar, o botão **JOGAR** fica disponível.

A ISO não é copiada, enviada nem armazenada pelo launcher. Somente os arquivos extraídos ficam no
computador, na pasta de dados indicada em **Configurações → Pastas**.

## Onde ficam os arquivos

A variável `DISNEY_DATA_ROOT` permite apontar a pasta de dados explicitamente (útil para
instalações em outro disco e para testes automatizados); nesse modo o runtime continua sendo
procurado ao lado do executável.

O launcher detecta dois modos:

| Modo | Quando é usado | Dados do jogo | Runtime | Assets | Saves | Logs |
| --- | --- | --- | --- | --- | --- | --- |
| Portátil | A pasta contém `original\extracted`, `runtime` + `assets\native` ou o marcador `launcher.portable` | `original\extracted` no checkout ou `game` no ZIP portátil | `runtime` | `assets` | `mc0` dentro da pasta do jogo | `analysis` |
| Instalado | Executável instalado sem esses diretórios | `%LOCALAPPDATA%\HighSky\DisneySkate\game` | `runtime` ao lado do executável | `%LOCALAPPDATA%\HighSky\DisneySkate\assets` | `%LOCALAPPDATA%\HighSky\DisneySkate\saves\profile-1` | `%LOCALAPPDATA%\HighSky\DisneySkate\logs` |

No modo instalado, os dados do usuário ficam fora da pasta de instalação para sobreviver a
atualizações. O arquivo de configuração é `analysis\launcher\config.ini` no modo portátil e
`%LOCALAPPDATA%\HighSky\DisneySkate\launcher\config.ini` no modo instalado.

## Validação da imagem

O launcher recusa:

- imagens sem ISO9660 reconhecível ou com `SYSTEM.CNF` vazio;
- imagens cujo `BOOT2` não seja exatamente `cdrom0:\SLUS_206.07;1`, ou que declarem `BOOT2`/`VMODE` duplicado;
- imagens PAL ou de outra região (`VMODE` diferente de `NTSC`);
- executáveis vazios, truncados, que não sejam ELF MIPS de 32 bits ou cujo SHA-256 não corresponda à revisão esperada;
- imagens sem os arquivos de dados ou módulos IOP obrigatórios, ou com esses arquivos vazios/truncados.

O SHA-256 de `SLUS_206.07` é comparado com a referência
`2fe6ce3a08aade170be27ea221137c96aa15083a408d61856f605f1a40f321fc` e registrado em
`analysis\launcher\install-state.ini`. O hash da ISO inteira não é calculado porque a imagem tem
vários gigabytes e não é necessária para identificar a revisão.

### Manifesto do pacote

Ao final da instalação, o launcher grava atomicamente `package-manifest.sha256` na pasta de
estado (`analysis\launcher` no modo portátil ou `%LOCALAPPDATA%\HighSky\DisneySkate\launcher`
no modo instalado). Cada linha registra SHA-256, tamanho e caminho lógico de um arquivo em
`runtime/`, `game/` ou `assets/`; saves não entram no manifesto.

Antes de liberar **Jogar**, uma checagem rápida confere o runtime e as DLLs como PE x64, o
cabeçalho e o SHA-256 do ELF, os arquivos essenciais, os módulos IOP, o catálogo de áudio e os
assets nativos. Ela não recalcula os hashes de todo o manifesto. O comando `--verificar` faz a
auditoria integral: caminhos seguros, entradas duplicadas, arquivos ausentes ou extras, tamanhos e
SHA-256. Nessa auditoria, manifesto ausente ou qualquer divergência mantém o pacote inválido. Uma
nova instalação regenera o manifesto.

## Preparação de assets e runtime

O launcher recebe um runtime já compilado e usa o pipeline próprio
(`scripts\Preparar-Dados-Nativos.ps1`, `scripts\Instalar-Audios-Originais.py` e
`scripts\Recuperar-Trilha-ISO.ps1`) para gerar assets a partir da ISO. O pacote de distribuição
inclui esses scripts e ferramentas pré-compiladas em `tools\`: Python 3.12.10 incorporado,
COL8 autocontido, recuperador de música nativo e Neversoft Multitool CLI v1.3.4. Não inclui ISO,
dados extraídos, áudio derivado, assets gerados ou saves. Efeitos, vozes e músicas são convertidos
dos dados da própria ISO selecionada.

Quando os assets e o áudio ainda não estão prontos, o launcher executa o preflight **antes da
extração**:

- procura os scripts em `scripts\` (ou em `source_root`, definido no `config.ini`);
- se o conjunto de ferramentas estiver completo, usa os executáveis incorporados sem consultar
  Python, .NET, CMake ou Visual Studio no `PATH`;
- se faltar alguma ferramenta, mantém o fallback de desenvolvimento: Python 3.9–3.12, SDK .NET 9,
  Neversoft Multitool CLI v1.3.4, CMake 3.21+ e Visual Studio 2022 com C++;
- informa o arquivo ou dependência ausente e interrompe sem começar a extrair.

O launcher usa CRT estática. As DLLs da CRT do runtime ficam em `runtime\`. O cache do .NET e os
temporários do fallback de desenvolvimento ficam em `logs\preparation`
(`analysis\preparation` no modo portátil), fora de `build\setup`. Cada execução do fallback do
conversor COL v8 usa um `NUGET_SCRATCH` próprio e um `TEMP`/`TMP` isolado para não herdar bloqueios
compartilhados do Windows; o build usa `--disable-build-servers`. Se o build falhar, a mensagem
aponta o `col8-build.log` da pasta de preparação. Uma instalação completa não passa por esse
fallback.

A validação dos dados de áudio extraídos pode ser executada sem criar ou substituir assets:

```powershell
python scripts\Instalar-Audios-Originais.py . --game-dir original\extracted --validate-only
```

O modo `--validate-only` confere `STREAMS`/`MUSIC`, os intervalos dos índices e os VAGs já
preparados em `assets\source\wad`. Se os assets e o catálogo de áudio já estiverem completos, o
preflight e a conversão são ignorados. Um checkout completo continua necessário para operações de
runtime não cobertas pelo pacote pré-compilado.

Em 26/11/2026, após a desinstalação, o fluxo foi executado no projeto real: 393 efeitos VAG,
855 streams e 37 músicas resultaram em 1.285 entradas únicas, sem erros de conversão, arquivos
ausentes ou músicas pendentes. O launcher exige as 37 entradas `music/` no catálogo para não
tratar uma preparação parcial como concluída. A preparação nativa compilou o conversor COL v8
sem erros e converteu 272 arquivos de colisão.

O ciclo completo foi exercitado pela interface em 26/11/2026: **Configurações → Desinstalar
jogo → Sim** removeu os dados do jogo, os assets, `build\setup`, o manifesto e o estado de
instalação; os saves permaneceram preservados e os hashes do runtime e do `config.ini` não
mudaram. A ISO foi selecionada no diálogo nativo do Windows e **Instalar jogo** restaurou as
1.285 entradas de áudio. A auditoria final `--verificar` conferiu 7.566 arquivos com tamanho e
SHA-256, e o CTest Release passou 1/1 em 5,35 segundos.

Em 27/11/2026, o pacote 0.5.0 gerado para distribuição foi instalado em pastas isoladas, sem
ferramentas de desenvolvimento no `PATH`. A instalação da ISO retornou 0, usando somente as
ferramentas incluídas, e `--verificar` retornou 0 após auditar 7.569 arquivos. O launcher abriu
na tela pronta e o runtime foi iniciado por Enter; a janela OpenGL permaneceu ativa por 12
segundos antes de o teste fechá-la, e o launcher foi encerrado com código 0. A variante
`-TestInstall` foi usada porque o ambiente
restringe a escrita em `HKCU`; o instalador oficial ainda precisa do teste de registro e atalhos
em uma conta sem essa restrição.

## Configurações

| Opção | Efeito | Variável de ambiente aplicada ao runtime |
| --- | --- | --- |
| Qualidade de renderização 1x/2x/3x | Resolução interna do GS | `PS2X_GS_HW_SCALE` |
| Renderizador GPU/Software | Backend de renderização | `DISNEY_RENDERER` |
| V-Sync nativo | Sincronização com o refresh | `DISNEY_NATIVE_VBLANK` |
| Diagnóstico de travamentos | Relatórios de travamento | `DISNEY_HANG_DIAGNOSTICS` / `DISNEY_HANG_OUTPUT` |
| Verificar atualizações | Consulta ao GitHub ao abrir | — |
| Desinstalar jogo | Remove dados e assets gerados após confirmação; preserva dados do usuário | — |

O runtime continua com resolução de janela livremente redimensionável; a escala altera a
resolução interna do GS. As opções valem na próxima vez que o jogo for aberto.

O arquivo `config.ini` também aceita `source_root`, usado para apontar a um checkout do projeto
quando o launcher está instalado separadamente. O caminho é salvo em UTF-8.

### Desinstalação

**Configurações → Desinstalar jogo** remove o conteúdo da pasta de dados do jogo, os assets
gerados, os intermediários de `build\setup`, o manifesto e o registro de instalação. O botão fica
bloqueado enquanto o jogo está aberto e a operação só começa depois de uma confirmação com **Não**
como opção padrão.

O launcher, o runtime, os saves, o `config.ini`, os logs, os relatórios e as ferramentas são
preservados. No modo portátil, a raiz do jogo (incluindo `game\mc0` nesse layout) e
`original\extracted\mc0` permanecem, e `assets\native` é recriada vazia para manter a detecção
do layout. Links dentro da área removida são
apagados sem seguir seus destinos. Falhas transitórias de acesso ou compartilhamento são
repetidas por alguns segundos enquanto o Windows fecha os handles; um bloqueio permanente
continua interrompendo a operação sem remover saves ou runtime. Ao terminar, é preciso
selecionar e instalar a ISO novamente para restaurar o jogo.

## Linha de comando

```text
"Disney's Extreme Skate Adventure.exe"                     abre o launcher
"..." --verificar                                          audita o pacote e retorna 0/2
"..." --atualizar-manifesto                                 regera o manifesto do pacote atual
"..." --iso <imagem> --verificar                           valida somente a ISO
"..." --instalar <imagem>                                  instala sem interface
"..." --diagnostico                                        sessão com diagnósticos
"..." --versao                                             mostra a versão
```

A interface usa uma checagem rápida dos componentes para abrir sem recalcular todos os hashes.
`--verificar` usa essas mesmas checagens e também confere tamanho e SHA-256 de todos os arquivos
do manifesto. Código de saída `0` significa pacote válido; `2`, falha na verificação ou imagem
inválida. Após recompilar o runtime, execute `--atualizar-manifesto` para manter a auditoria
integral sincronizada sem repetir a instalação.

## Atualizações

A tela **Atualizações** consulta `releases/latest` do repositório oficial no GitHub por HTTPS
com WinHTTP. Se houver uma versão mais nova, mostra as notas e permite abrir a página da versão ou
baixar um instalador `.exe` publicado como asset. O nome esperado é derivado da tag do release
(`v0.5.0` seleciona `DisneySkateSetup-0.5.0.exe`), e a URL precisa conter exatamente essa tag e
esse asset no repositório oficial. O tamanho anunciado pelo release é conferido durante o download,
limitado a 512 MiB, e o instalador precisa ter uma assinatura Authenticode confiável do Windows
antes de ser executado. O download é salvo em `%TEMP%` e só é executado depois de uma confirmação;
o launcher se fecha antes para permitir a substituição dos arquivos.

Ao clicar em **Verificar novamente**, o launcher abre um modal com animação de carregamento. Se a
versão instalada já for a mais recente ou não houver releases publicados, o modal muda para
**Nenhuma atualização encontrada**, com o botão **Entendi**. A checagem automática ao abrir não abre
esse modal.

Não há envio de dados do jogo, da ISO ou de telemetria. A consulta é somente de metadados
públicos.

## Logs e diagnóstico

- `analysis\launcher.log` (ou `logs\launcher.log` no modo instalado): decisões do launcher,
  validação, extração, compilação e inicialização do jogo.
- `analysis\game-session.log`: saída do runtime.
- `analysis\hang-reports\`: relatórios gerados quando o diagnóstico de travamentos está ligado.

A tela **Logs e diagnóstico** mostra os dois logs, permite abrir a pasta e copiar um resumo com a
versão, o layout, o estado do pacote e as últimas linhas do log. Nada é enviado automaticamente.

## Instalador

Com Visual Studio 2022 C++, .NET SDK 9, CMake e Inno Setup 6 instalados:

```powershell
.\scripts\Empacotar-Instalador.ps1
```

O script compila o launcher e seus testes, publica o COL8 autocontido, compila o recuperador de
música, baixa Python e Neversoft Multitool com hashes fixados, monta um staging limpo e valida
allowlist, PE x64, hashes e versão. Gera:

- `dist\DisneySkateSetup-0.5.0.exe`;
- `dist\DisneySkate-0.5.0-portable.zip`;
- `dist\SHA256SUMS.txt`.

O setup instala launcher, runtime, ferramentas, documentação, avisos e atalhos. **Não** inclui ISO,
dados extraídos, assets gerados, saves, logs ou PDBs. A instalação é por usuário
(`PrivilegesRequired=lowest`) e os dados ficam em `%LOCALAPPDATA%\HighSky\DisneySkate`.

Para assinar, informe `-PfxPath` e defina a senha em `DISNEY_SIGNING_PASSWORD`. O Setup.exe de
release ainda precisa de assinatura confiável antes de ser distribuído publicamente. Em ambientes
que restringem a escrita em `HKCU`, use `-TestInstall` para gerar uma variante sem registro de
desinstalador; ela não é um artefato de release.

## Compilar e testar

O launcher compila sem os dados do jogo, sem PS2Recomp e sem o runtime AOT:

```powershell
.\scripts\Compilar-Launcher.ps1
```

Ou manualmente:

```powershell
cmake -S . -B build/launcher -G "Visual Studio 17 2022" -A x64 -DDISNEY_BUILD_LAUNCHER_ONLY=ON
cmake --build build/launcher --config Release --target DisneySkateLauncher DisneyLauncherTests --parallel 2
ctest --test-dir build/launcher -C Release --output-on-failure
```

Os testes cobrem JSON, comparação de versões, configuração, detecção de layout, verificação de
pacote e manifesto, desinstalação segura e preservação de saves, processos, leitura de ISO e
extração. Para executar a integração com uma ISO local:

```powershell
$env:DISNEY_TEST_ISO = "C:\caminho\para\Disney's Extreme Skate Adventure (USA).iso"
$env:DISNEY_TEST_EXTRACTED = "C:\caminho\para\original\extracted"
build\launcher\launcher-tests\Release\DisneyLauncherTests.exe
```

O teste de integração confere assinatura da ISO, serial, ELF, hash de referência do executável e
compara os arquivos extraídos com os dados locais.

## Limitações atuais

- O Setup.exe local ainda não possui assinatura Authenticode confiável; o atualizador só executa
  instaladores assinados.
- O pacote de distribuição incorpora as ferramentas de preparação. O fallback de desenvolvimento
  continua disponível quando alguma delas está ausente.
- O runtime AOT completo não é reproduzível por um clone limpo; o release workflow consome um ZIP
  de runtime confiável com hash e commit verificados.
- A revisão dos direitos de redistribuição do runtime AOT é obrigatória antes de publicar um
  Release.
- Imagens BIN/CUE ainda não têm leitor próprio; use ISO.
- A atualização de componentes baixa e executa um instalador publicado; não há atualização
  diferencial nem atualização automática do ZIP portátil.
- Nenhum pacote inclui ISO, dados extraídos, saves, logs ou capas soltas do jogo; a capa do launcher é incorporada ao executável.

## Direitos

Projeto não oficial, sem vínculo com os titulares de Disney, Activision, Pixar ou PlayStation.
O jogo, personagens, músicas e a capa pertencem aos respectivos detentores. Use somente uma cópia
legal do jogo.
