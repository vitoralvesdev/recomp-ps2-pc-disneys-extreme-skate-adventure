# Disney's Extreme Skate Adventure — Recompilação nativa para Windows

![Capa de Disney's Extreme Skate Adventure para PlayStation 2](docs/capa-jogo.png)

Projeto da comunidade, **em desenvolvimento**, para recompilar nativamente o jogo de
**PlayStation 2 Disney's Extreme Skate Adventure** para Windows x64. O candidato principal
usa código do jogo recompilado antecipadamente e serviços adaptados para o PC: **não é um
emulador**, não depende do PCSX2 e não interpreta as instruções do jogo em tempo de execução.
Há um protótipo legado separado para pesquisa, que não é o candidato principal.

O jogo, seus personagens, marcas e a capa pertencem aos respectivos detentores de direitos.
Este é um projeto não oficial, sem vínculo com eles.

O projeto combina quatro camadas próprias:

| Camada | O que faz |
| --- | --- |
| Runtime do host | Loop principal, scheduler, memória RDRAM/VRAM, IRQs, DMA, bridges de sistema |
| Código do jogo (AOT) | Funções do EE (R5900) e microprogramas de VU0/VU1 recompilados para C++ |
| GPU | GS (Graphics Synthesizer) traduzido para OpenGL 3.3, com merge de batches e apresentação em janela |
| Mídia | Áudio nativo e IOP reimplementados no host; decodificador de vídeo ligado, cutscenes ainda não validadas |

O projeto inclui um **launcher nativo** em C++/Win32 para instalar e abrir o jogo. Na primeira
execução ele valida a ISO americana (`SLUS_206.07`), lê a ISO9660 sem depender de 7-Zip, extrai
os arquivos de forma incremental e mostra o progresso real da preparação. Depois de preparado, o
launcher valida o pacote, ajusta o renderer, a escala e a pasta de saves, abre a janela do jogo e
acompanha a sessão. A instalação completa ainda depende de ferramentas e arquivos gerados que não
estão no repositório; o launcher informa exatamente qual etapa indisponível impediu a conclusão.
Detalhes em [docs/launcher.md](docs/launcher.md).

---

## Status de desenvolvimento

Baseado na última melhoria de desempenho e nas medições locais de **23/11/2026**; os resultados podem variar entre computadores.
O projeto ainda não foi validado do início ao fim.

| Área | Situação | O que funciona ou falta |
| --- | --- | --- |
| Inicialização e menus | Funciona nos testes | Menu e seleção de personagem aparecem nas sessões registradas. |
| Entrada em fase | Funciona nos testes | Há registros do HUB e de entrada em uma fase de Toy Story; outras fases precisam de testes. |
| Teclado e controle | Funciona, precisa melhorar | Há mapeamento de teclado e controle; compatibilidade com todos os modelos não foi validada. |
| Gameplay e desempenho | Melhora significativa; em validação | **59,98 FPS lógicos em média** em Toy Story Bedroom (8 amostras entre 8400–10500 frames; faixa 59,82–60,14). Menu permanece perto de 60 FPS. Outras fases e comparação controlada com o HUB ainda precisam de validação. |
| Áudio | Funciona, precisa melhorar | Sons e músicas foram iniciados nos registros. O log recente mostra WAVs de efeitos vazios no catálogo (por exemplo, alguns sons Conc/Metal); a recuperação desses recursos ainda falta. |
| Gráficos | Funciona, precisa melhorar | Menu e cenas aparecem; testes sintéticos de textura passaram, mas fidelidade geral e blending precisam de validação. |
| Colisão | Dados do protótipo ampliados; gameplay ainda não validado | O preparador agora converte os arquivos COL v8 de todas as fases extraídas. A equivalência da colisão em gameplay e os relatos de rampas/objetos ainda precisam de validação. |
| Carregamento | Problemas conhecidos | Algumas fases podem parar ao carregar; houve uma espera inicial longa nos testes. |
| Vídeos e cutscenes | Pendente de validação | O decodificador está ligado, mas as cutscenes não foram validadas no fluxo recente. |
| Saves | Pendente de validação completa | Há teste isolado do serviço, mas falta confirmar salvar e carregar uma partida completa. |
| Build limpo do código versionado | Launcher passou localmente e em CI | O alvo `DISNEY_BUILD_LAUNCHER_ONLY=ON` compila somente o launcher e seus testes, sem `tools/`, ISO, raylib ou fontes geradas. O runtime AOT completo ainda não compila de um clone limpo. |
| Launcher nativo e instalação | Funcional no ambiente local; empacotamento pendente | Valida a ISO NTSC-U, extrai 42 arquivos (2,16 GB) com progresso, detecta dados já prontos, configura o runtime, abre e acompanha o jogo, salva configurações e mostra logs. A preparação de assets e a compilação do runtime ainda usam os scripts locais; Setup.exe e download de ferramentas não foram implementados. |

### Melhoria de FPS em gameplay (23/11/2026)

O runtime AOT recebeu a correção de inicialização do registrador constante VF0 e dos acessos
EE à memória de dados do VU0, além do ajuste SIMD da normalização VU. Com esses ajustes, a
sessão instrumentada em `ToyStory_Bedroom` chegou perto do limite de 60 FPS lógicos durante
todo o trecho de gameplay amostrado. A medição confirma melhora expressiva nessa fase; ainda
falta repetir o mesmo benchmark controlado no HUB e nas demais fases para estabelecer o ganho
geral do projeto.

| Métrica instrumentada em ToyStory_Bedroom | Média (8 amostras, frames 8400–10500) |
| --- | ---: |
| FPS lógico | 59,98 (59,82–60,14) |
| VIF1 | 36,9% |
| VU1 | 26,2% |
| GIF/GS | 9,4% |
| Worker GPU ocupado | 72,4% |
| Espera de GPU | 25,5% |

O log confirmou `Loading Scene ToyStory_Bedroom ...... done`, `CAREER_STARTED` e `starting a run`.
As oito amostras de gameplay foram coletadas entre os frames 8400 e 10500. Como a sessão usou
instrumentação e uma fase diferente dos registros anteriores, os números abaixo documentam o
resultado observado; uma comparação A/B geral ainda requer repetir o cenário antigo com o mesmo
build e configuração.

### Problemas conhecidos

- O FPS das fases além de Toy Story Bedroom ainda precisa de uma medição controlada após a
  melhoria de 23/11/2026.
- Hitch único de ~15 s ao entrar na fase (loader do jogo, antes do primeiro frame de gameplay).
  A causa ainda não foi isolada. Leituras dos arquivos de fase foram registradas como 0 ms;
  falta medir o trabalho síncrono de parsing/construção do loader por etapa.
- Áudio validado no menu e no hub; alguns bancos de som de fase ainda registram
  `couldn't load` no log durante o carregamento. A inspeção confirmou que vários WAVs
  correspondentes no catálogo têm tamanho zero; os sons de origem precisam ser recuperados.
- Cutscenes em vídeo (`.PSS`) ainda não foram exercitadas pelo fluxo testado.
- Colisão de rampas e objetos no jogo recompilado ainda não validada, principalmente nas fases de Toy Story. O kernel VU0 de interseção conferiu em 120 mil chamadas registradas, mas o caso visual problemático não aparece nesse corpus. O protótipo nativo agora pode preparar os COL v8 de todas as fases; isso não comprova correção no runtime do jogo.
- Algumas fases podem parar durante o carregamento.

Relate defeitos informando a fase, o personagem, a ação que reproduz o problema e o log
`analysis/game-session.log` gerado pelo iniciador. **Nunca anexe ISO, WAD, saves ou outros dados
originais do jogo.**

---

## Instalação e execução

**O repositório não inclui o executável pronto, o runtime nem os dados do jogo.** É necessária
uma cópia legal da edição americana de PS2. O launcher compila sem os dados, mas a preparação dos
assets e a compilação do runtime ainda dependem dos scripts e de fontes geradas do ambiente de
desenvolvimento; um clone limpo não conclui a instalação sozinho.

### 1. Obtenha ou compile o launcher

Para desenvolver, compile somente o launcher (não exige ISO, raylib, PS2Recomp nem código AOT):

```powershell
.\scripts\Compilar-Launcher.ps1
```

O executável é gerado na raiz como `Disney's Extreme Skate Adventure.exe`. O script também roda
os testes do launcher. Para testar com uma ISO local:

```powershell
.\scripts\Compilar-Launcher.ps1 -Iso "C:\Jogos\Disney's Extreme Skate Adventure (USA).iso"
```

### 2. Primeira execução

Dê **duplo clique em `Disney's Extreme Skate Adventure.exe`**. A tela inicial mostra o estado dos
componentes e permite:

1. **Selecionar ISO** — escolha a imagem americana de PlayStation 2 (NTSC-U).
2. O launcher valida `SYSTEM.CNF`, a serial `SLUS_206.07`, a região, o ELF MIPS e os arquivos de
   dados antes de escrever qualquer coisa.
3. **Instalar jogo** — as cinco etapas aparecem com progresso real: verificar imagem, extrair
   dados, preparar assets, preparar runtime e verificar o pacote.
4. **JOGAR** — quando o pacote está pronto.

A extração lê a própria ISO9660, sem 7-Zip, e é incremental: arquivos já presentes com o tamanho
esperado não são copiados de novo. São cerca de 2,16 GB de dados extraídos; reserve ao menos
2,3 GB livres. A ISO não é copiada nem enviada. Em um checkout, os arquivos vão para
`original/extracted/`; em uma instalação, para `%LOCALAPPDATA%\HighSky\DisneySkate\game`, com
saves e logs fora da pasta do programa.

### 3. Execução e diagnóstico

Com o pacote pronto, o launcher configura renderer, escala interna, V-Sync, pasta de saves e
diagnósticos, abre o runtime e mostra quando o jogo está em execução. Ao fechar o jogo, o launcher
volta ao estado pronto.

Opções de linha de comando:

| Comando | Efeito |
| --- | --- |
| `"Disney's Extreme Skate Adventure.exe" --verificar` | Verifica o pacote e retorna 0 (pronto) ou 2 (incompleto); não testa gameplay |
| `"Disney's Extreme Skate Adventure.exe" --iso <imagem> --verificar` | Valida somente a ISO escolhida |
| `"Disney's Extreme Skate Adventure.exe" --instalar <imagem>` | Instala sem interface, útil para automação |
| `"Disney's Extreme Skate Adventure.exe" --diagnostico` | Sessão normal com relatórios em `analysis/hang-reports` |
| `"Disney's Extreme Skate Adventure.exe" --versao` | Mostra a versão do launcher |

Também é possível abrir o runtime diretamente, a partir da raiz do projeto:

```bat
runtime\DisneySkateRuntime.exe original\extracted\SLUS_206.07
```

### 4. Instalador opcional

Com o Inno Setup 6 instalado, é possível gerar um `Setup.exe` que instala o launcher, o runtime,
a documentação e os atalhos, sem incluir ISO, dados do jogo ou assets:

```powershell
.\scripts\Empacotar-Instalador.ps1
```

O resultado fica em `dist\DisneySkateSetup-0.3.0.exe`. O instalador ainda não é assinado e não
resolve a preparação de assets: essa etapa continua sendo feita localmente a partir da ISO.

### 5. Se o jogo não abrir

O pacote local precisa das DLLs de mídia usadas pelo runtime. Em um Windows limpo, se aparecer uma mensagem
citando `VCRUNTIME140.dll` ou `MSVCP140.dll`, instale o **Microsoft Visual C++ Redistributable x64**
mais recente e reinicie o jogo:

<https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist>

Atualize também o driver da GPU pelo fabricante. O runtime usa **OpenGL 3.3**.

O launcher grava `analysis\launcher.log`, `analysis\game-session.log` e os relatórios de
travamento. Em uma instalação, os mesmos arquivos ficam em
`%LOCALAPPDATA%\HighSky\DisneySkate\logs`. A tela **Logs e diagnóstico** mostra os dois logs e
permite copiar um resumo. Esses arquivos são locais e ignorados pelo Git.

### Controles

Teclado:

| Ação | Tecla |
| --- | --- |
| Direção | WASD ou setas |
| Confirmar / botão X | Espaço ou X |
| Voltar / círculo | C |
| Start | Enter |
| Select | Tab |
| L1 / R1 | Q / E |
| L2 / R2 | Shift esquerdo / Shift direito |

O runtime tenta usar o primeiro controle reconhecido pelo raylib. O mapeamento segue as posições
dos botões de um controle tipo Xbox; o teclado continua ativo. A compatibilidade varia conforme
o controle e ainda precisa de mais testes.

### Requisitos

O alvo atual é **Windows x64** com GPU e drivers compatíveis com **OpenGL 3.3**. Reserve cerca de
**2,3 GB livres** para os dados extraídos da ISO. Os requisitos mínimos de processador e memória
ainda não foram validados em diferentes computadores.

---

## Estrutura do repositório

```text
src/host/          Runtime do host: loop principal, scheduler, memória, IRQs, bridges, áudio/vídeo
src/launcher/      Launcher Win32: ISO9660, validação, instalação, interface, configurações e logs
native/original/   Port nativo: GS/OpenGL, VIF/GIF, VU, IOP, áudio, input, oclusão
native/            Conversor de assets e testes nativos de colisão
tests/             Testes do runtime e dos drivers gráficos
scripts/           Preparação de dados, compilação e análise
recomp/            Configuração da recompilação (pontos de entrada e bridges)
docs/              Documentação técnica, arquitetura e política de distribuição
```

O que **não** fica no Git:

- `original/`, `assets/`, `mc0/`, `mc1/` — dados do jogo e saves (direitos autorais).
- `runtime/`, `build/`, `dist/`, `*.exe`, `*.dll` — binários gerados.
- `analysis/`, capturas de gameplay, logs e dumps — diagnósticos locais; a capa ilustrativa é uma exceção.
- `src/generated-*/`, `src/recovered/`, `src/host/recovered_functions.cpp`,
  `recomp/output/`, `recomp/recovered-overlap-entries.json`,
  `native/original/iop/generated/`, `native/original/vu/generated/` — saídas geradas a partir
  dos dados originais.

O `.gitignore` ajuda a impedir novos envios acidentais, mas **não remove arquivos já rastreados
nem apaga o histórico**. Revise a lista e os commits antes de publicar. Consulte
[docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

---

## Desenvolvimento

O launcher tem um build isolado e reproduzível, sem ISO, raylib, PS2Recomp ou código AOT:

```powershell
.\scripts\Compilar-Launcher.ps1
# ou:
cmake -S . -B build/launcher -G "Visual Studio 17 2022" -A x64 -DDISNEY_BUILD_LAUNCHER_ONLY=ON
cmake --build build/launcher --config Release --target DisneySkateLauncher DisneyLauncherTests --parallel 2
ctest --test-dir build/launcher -C Release --output-on-failure
```

O workflow Windows executa esse mesmo alvo e os testes. A arquitetura e o fluxo de instalação
estão em [docs/launcher.md](docs/launcher.md).

O runtime AOT completo continua não reproduzível em um clone limpo: ele depende de PS2Recomp,
bibliotecas, dados de jogo e código recompilado gerados localmente. O comando histórico abaixo
funciona apenas no ambiente de desenvolvimento existente:

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

Veja [docs/BUILDING.md](docs/BUILDING.md); a documentação de
[arquitetura](docs/arquitetura-nativa.md) descreve também uma etapa histórica do projeto.

## Como colaborar

Ajuda com desempenho, gráficos, áudio, colisão, preparação de dados e testes é bem-vinda. Ao
relatar um problema, informe a fase, a ação, o resultado e as configurações do computador.
Revise logs antes de compartilhá-los para remover caminhos e dados pessoais; nunca envie dados
originais, saves ou arquivos privados.

O projeto contém componentes de terceiros com suas próprias licenças, incluindo GPL-3.0; a
proveniência e a licença global ainda precisam de revisão. A capa e os demais direitos do jogo
pertencem aos respectivos titulares.
