# Launcher nativo

O launcher é o executável `Disney's Extreme Skate Adventure.exe` produzido a partir de
`src/launcher`. Ele é uma aplicação Win32 nativa em C++20, com interface GDI+, e não depende de
PowerShell, Electron, 7-Zip ou de um navegador para a instalação normal.

Versão atual: **0.3.0**.

## O que ele faz

1. Identifica uma imagem de disco do PlayStation 2.
2. Valida a edição NTSC-U esperada: `SYSTEM.CNF`, `BOOT2 = cdrom0:\SLUS_206.07;1`,
   `VMODE = NTSC`, ELF MIPS little-endian e os arquivos `SKATE4.HED`, `SKATE4.WAD` e módulos IOP.
3. Lê a ISO9660 diretamente (imagens de 2048 bytes por setor e variantes cruas de 2352/2336)
   e extrai os arquivos para a pasta de dados. Não é necessário instalar o 7-Zip.
4. Prepara os assets nativos e o runtime. Em um checkout de desenvolvimento, delega aos scripts
   existentes do projeto; em um pacote pré-compilado, valida os componentes já presentes.
5. Verifica o pacote antes de liberar o botão **Jogar**.
6. Abre o runtime, configura o renderer, a escala, o V-Sync, a pasta de saves e os diagnósticos.
7. Mantém o launcher aberto enquanto o jogo roda e volta ao estado pronto quando o jogo fecha.

A extração é incremental: arquivos já presentes com o tamanho esperado não são copiados de novo.
Uma atualização do port pode substituir o launcher e o runtime sem pedir a ISO novamente, desde
que a pasta de dados do jogo continue válida.

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
| Portátil | A pasta do executável contém `original\extracted` ou `runtime` + `assets\native` | `original\extracted` | `runtime` | `assets` | `original\extracted\mc0` | `analysis` |
| Instalado | Executável instalado sem esses diretórios | `%LOCALAPPDATA%\HighSky\DisneySkate\game` | `runtime` ao lado do executável | `%LOCALAPPDATA%\HighSky\DisneySkate\assets` | `%LOCALAPPDATA%\HighSky\DisneySkate\saves\profile-1` | `%LOCALAPPDATA%\HighSky\DisneySkate\logs` |

No modo instalado, os dados do usuário ficam fora da pasta de instalação para sobreviver a
atualizações. O arquivo de configuração é `analysis\launcher\config.ini` no modo portátil e
`%LOCALAPPDATA%\HighSky\DisneySkate\launcher\config.ini` no modo instalado.

## Validação da imagem

O launcher recusa:

- imagens sem ISO9660 reconhecível;
- imagens cujo `SYSTEM.CNF` não aponta para `SLUS_206.07`;
- imagens PAL ou de outra região (`VMODE` diferente de `NTSC`);
- executáveis que não sejam ELF MIPS de 32 bits;
- imagens sem os arquivos de dados ou módulos IOP obrigatórios.

O SHA-256 de `SLUS_206.07` é calculado e registrado em
`analysis\launcher\install-state.ini`. O hash da ISO inteira não é calculado porque a imagem tem
vários gigabytes e não é necessária para identificar a revisão.

## Preparação de assets e runtime

A instalação de um jogador não deve exigir Visual Studio. O launcher foi desenhado para receber
um pacote de runtime já compilado e para gerar os dados derivados do disco localmente. No estado
atual do projeto, a geração dos assets ainda usa o pipeline de desenvolvimento
(`scripts\Preparar-Dados-Nativos.ps1`) e o runtime AOT completo ainda não é reproduzível em um
clone limpo. Quando esses componentes não estão presentes, o launcher:

- procura os scripts em `scripts\` (ou em `source_root`, definido no `config.ini`);
- executa a preparação com PowerShell, mostrando a saída real na tela de progresso;
- interrompe com uma mensagem clara e mantém os arquivos já extraídos para uma nova tentativa.

Isso permite evoluir o launcher agora e substituir os scripts por downloads/componentes
versionados depois, sem mudar a interface.

## Configurações

| Opção | Efeito | Variável de ambiente aplicada ao runtime |
| --- | --- | --- |
| Qualidade de renderização 1x/2x/3x | Resolução interna do GS | `PS2X_GS_HW_SCALE` |
| Renderizador GPU/Software | Backend de renderização | `DISNEY_RENDERER` |
| V-Sync nativo | Sincronização com o refresh | `DISNEY_NATIVE_VBLANK` |
| Diagnóstico de travamentos | Relatórios de travamento | `DISNEY_HANG_DIAGNOSTICS` / `DISNEY_HANG_OUTPUT` |
| Verificar atualizações | Consulta ao GitHub ao abrir | — |

O runtime continua com resolução de janela livremente redimensionável; a escala altera a
resolução interna do GS. As opções valem na próxima vez que o jogo for aberto.

O arquivo `config.ini` também aceita `source_root`, usado para apontar a um checkout do projeto
quando o launcher está instalado separadamente.

## Linha de comando

```text
"Disney's Extreme Skate Adventure.exe"                     abre o launcher
"..." --verificar                                          verifica o pacote e retorna 0/2
"..." --iso <imagem> --verificar                           valida somente a ISO
"..." --instalar <imagem>                                  instala sem interface
"..." --diagnostico                                        sessão com diagnósticos
"..." --versao                                             mostra a versão
```

`--verificar` usa o mesmo conjunto de checagens da interface. Código de saída `0` significa
pacote pronto; `2`, pacote incompleto ou imagem inválida.

## Atualizações

A tela **Atualizações** consulta `releases/latest` do repositório oficial no GitHub por HTTPS
com WinHTTP. Se houver uma versão mais nova, mostra as notas e permite abrir a página da versão ou
baixar um instalador `.exe` publicado como asset. O download é salvo em `%TEMP%` e só é executado
depois de uma confirmação; o launcher se fecha antes para permitir a substituição dos arquivos.

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

Com o Inno Setup 6 instalado:

```powershell
.\scripts\Empacotar-Instalador.ps1
```

O script compila o launcher e gera `dist\DisneySkateSetup-0.3.0.exe`, que instala o launcher, o
runtime compilado, a documentação e os atalhos. O setup **não** inclui ISO, dados extraídos,
assets gerados ou saves. A instalação é por usuário (`PrivilegesRequired=lowest`) e os dados
ficam em `%LOCALAPPDATA%\HighSky\DisneySkate`.

O instalador de release ainda precisa de assinatura de código antes de ser distribuído
publicamente. Em ambientes que restringem a escrita em `HKCU` (como sandboxes de teste), o
desinstalador não pode ser registrado; o pacote de arquivos em si pode ser validado com
`/DLAUNCHER_TEST_INSTALL` no ISCC.

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
pacote, processos, leitura de ISO e extração. Para executar a integração com uma ISO local:

```powershell
$env:DISNEY_TEST_ISO = "C:\caminho\para\Disney's Extreme Skate Adventure (USA).iso"
$env:DISNEY_TEST_EXTRACTED = "C:\caminho\para\original\extracted"
build\launcher\launcher-tests\Release\DisneyLauncherTests.exe
```

O teste de integração confere assinatura da ISO, serial, ELF, hash do executável e compara os
arquivos extraídos com os dados locais.

## Limitações atuais

- O instalador Setup.exe (Inno Setup) ainda não é gerado neste repositório.
- O download automático de ferramentas e dependências de compilação não foi implementado; o
  launcher chama o pipeline local quando ele existe.
- Imagens BIN/CUE ainda não têm leitor próprio; use ISO.
- A atualização de componentes baixa e executa um instalador publicado; não há atualização
  diferencial nem verificação de assinatura do instalador.
- O launcher não distribui ISO, dados do jogo, assets derivados, runtime compilado ou saves.

## Direitos

Projeto não oficial, sem vínculo com os titulares de Disney, Activision, Pixar ou PlayStation.
O jogo, personagens, músicas e a capa pertencem aos respectivos detentores. Use somente uma cópia
legal do jogo.
